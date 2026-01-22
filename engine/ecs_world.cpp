// 
//  ecs_world.cpp rbashkort 01/01/2026
//

#include "ecs_world.h"
#include "components.h"
#include "physics.hpp"

#include <GLFW/glfw3.h>
#include <flecs/addons/cpp/entity.hpp>
#include <flecs/addons/cpp/iter.hpp>
#include <flecs/addons/cpp/mixins/pipeline/decl.hpp>
#include <flecs/addons/cpp/ref.hpp>
#include <math.h>
#include <cstdio>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

template<typename... Components>
static void register_components(flecs::world& w) {
    (w.component<Components>(), ...);
}

ECSWorld::ECSWorld() : world() {}

void ECSWorld::init() {
    printf("[Engine] ECS world init called\n");

    register_components<E_Transform, E_Velocity, E_Color, E_Texture, E_Sprite, E_Camera,
        E_InputState, E_Clickable, E_EffectHover, E_EffectShadow, E_EffectOutline, E_EffectTranspare,
        E_Mass, E_PhysicsMaterial, E_Collider, E_CollisionEvent, E_Gravity, E_WindowSize>(world);
    
    E_InputState initState;
    memset(&initState, 0, sizeof(E_InputState));
    world.set<E_InputState>(initState);
    printf("[Engine] Components registered\n");

    qColliders_ = world.query<E_Transform, E_Collider>();
    qCamera_ = world.query<E_Transform, E_Camera>();

    // --- Move System ---
    world.system<E_Transform, E_Velocity>("MoveSystem")
        .each([](flecs::entity e, E_Transform& t, E_Velocity& v) {
            float dt = e.world().delta_time();
            if (!v.freeze) {
                t.x += v.vx * dt;
                t.y += v.vy * dt;
            }
        });

    // --- Gravity System ---
    world.system<E_Velocity, E_Gravity>("GravitySystem")
        .each([](flecs::entity e, E_Velocity& v, E_Gravity& g) {
            if (g.work && !v.freeze) {
                float dt = e.world().delta_time();
                v.vy += g.a * dt;
            }
        }); 

    // --------------------------------------------------------
    // Helpers (Local lambdas)
    // --------------------------------------------------------
    auto aabbHalfExtents = [](const E_Collider& c) -> Vec2 {
        if (c.type == ColliderType::Circle) return {c.radius, c.radius};
        float hx = c.width * 0.5f;
        float hy = c.height * 0.5f;
        float r = std::sqrt(hx*hx + hy*hy);
        return {r, r};
    };

    // --------------------------------------------------------
    // System 1: Build Grid (PreUpdate)
    // --------------------------------------------------------
    world.system<>("BuildBroadphaseGrid")
        .kind(flecs::PreUpdate)
        .run([this](flecs::iter& it) {
            grid_.clear();
            bigBodies_.clear();
            testedPairs_.clear();

            float dt = it.delta_time();

            qColliders_.each([&](flecs::entity e, E_Transform& t, E_Collider& c) {
                if (!c.active) return; // Skip inactive colliders

                float cx = t.x + c.offsetX;
                float cy = t.y + c.offsetY;
                
                if (std::isnan(cx) || std::isnan(cy)) return;

                float r = 0.0f;
                if (c.type == ColliderType::Circle) {
                    r = c.radius;
                } else {
                    float hx = c.width * 0.5f;
                    float hy = c.height * 0.5f;
                    r = std::sqrt(hx*hx + hy*hy);
                }
                
                Vec2 vel = {0, 0};
                const E_Velocity* v = e.has<E_Velocity>() ? &e.get<E_Velocity>() : nullptr;
                if (v) vel = {v->vx, v->vy};

                bool isBig = (r > (float)CELL_SIZE * 3.0f);
                if (isBig) {
                    bigBodies_.push_back(e.id());
                }

                float vx = vel.x * dt;
                float vy = vel.y * dt;
                float minX = cx - r + std::fmin(0.0f, vx);
                float maxX = cx + r + std::fmax(0.0f, vx);
                float minY = cy - r + std::fmin(0.0f, vy);
                float maxY = cy + r + std::fmax(0.0f, vy);

                int cMinX = (int)floorf(minX / (float)CELL_SIZE);
                int cMaxX = (int)floorf(maxX / (float)CELL_SIZE);
                int cMinY = (int)floorf(minY / (float)CELL_SIZE);
                int cMaxY = (int)floorf(maxY / (float)CELL_SIZE);
                
                if (cMaxX - cMinX > 100 || cMaxY - cMinY > 100) { 
                     grid_[hashCellGlobal((int)(cx/CELL_SIZE), (int)(cy/CELL_SIZE))].push_back(e.id());
                     return;
                }

                for (int x = cMinX; x <= cMaxX; ++x) {
                    for (int y = cMinY; y <= cMaxY; ++y) {
                         grid_[hashCellGlobal(x, y)].push_back(e.id());
                    }
                }
            });
        });

    // --------------------------------------------------------
    // System 2: Collision & Resolve (OnUpdate)
    // --------------------------------------------------------
    world.system<E_Transform, E_Collider>("CollisionSystem")
        .kind(flecs::OnUpdate)
        .run([this, &aabbHalfExtents](flecs::iter& it) {
            while (it.next()) {
                auto tArr = it.field<E_Transform>(0);
                auto cArr = it.field<E_Collider>(1);

                for (auto i : it) {
                    flecs::entity eA = it.entity(i);
                    E_Transform& tA = tArr[i];
                    E_Collider&  cA = cArr[i];

                    if (!cA.active) continue;

                    auto tryMarkPair = [&](flecs::entity_t a, flecs::entity_t b) -> bool {
                        if (a == b) return false;
                        if (a > b) std::swap(a, b);
                        return testedPairs_.insert(PairKey{a, b}).second;
                    };

                    auto resolvePair = [&](flecs::entity eA, E_Transform& tA, E_Collider& cA,
                                           flecs::entity eB, E_Transform& tB, E_Collider& cB) {
                        
                        if (cA.layer != cB.layer) return;
                        if (!cA.active || !cB.active) return;

                        const float ax = tA.x + cA.offsetX;
                        const float ay = tA.y + cA.offsetY;
                        const float bx = tB.x + cB.offsetX;
                        const float by = tB.y + cB.offsetY;

                        auto halfA = (cA.type == ColliderType::Circle) ? Vec2{cA.radius, cA.radius}
                                                                       : Vec2{cA.width * 0.5f, cA.height * 0.5f};
                        auto halfB = (cB.type == ColliderType::Circle) ? Vec2{cB.radius, cB.radius}
                                                                       : Vec2{cB.width * 0.5f, cB.height * 0.5f};

                        if (std::abs(ax - bx) > (halfA.x + halfB.x)) return;
                        if (std::abs(ay - by) > (halfA.y + halfB.y)) return;

                        Vec2 mtv = {0, 0};
                        bool collided = false;

                        const bool polyA = (cA.type == ColliderType::Rect || cA.type == ColliderType::Triangle);
                        const bool polyB = (cB.type == ColliderType::Rect || cB.type == ColliderType::Triangle);
                        const bool circleA = (cA.type == ColliderType::Circle);
                        const bool circleB = (cB.type == ColliderType::Circle);

                        if (polyA && polyB) {
                            PolyVerts vA = getVertices(tA, cA);
                            PolyVerts vB = getVertices(tB, cB);
                            collided = satPolyPoly(vA.data, vA.count, vB.data, vB.count, mtv);
                        }
                        else if (circleA && circleB) {
                            collided = satCircleCircle({ax, ay}, cA.radius, {bx, by}, cB.radius, mtv);
                        }
                        else if (circleA && polyB) {
                            PolyVerts vB = getVertices(tB, cB);
                            Vec2 mtvTemp = {0, 0};
                            if (satCirclePoly({ax, ay}, cA.radius, vB.data, vB.count, mtvTemp)) {
                                collided = true;
                                mtv = mul(mtvTemp, -1.0f);
                            }
                        }
                        else if (polyA && circleB) {
                            PolyVerts vA = getVertices(tA, cA);
                            collided = satCirclePoly({bx, by}, cB.radius, vA.data, vA.count, mtv);
                        }

                        if (!collided) return;
                        
                        bool isSensor = cA.isTrigger || cB.isTrigger;
                        it.world().entity().set<E_CollisionEvent>({eA, eB, isSensor});
                        
                        if (isSensor) return;

                        float invMassA = 1.0f;
                        float invMassB = 1.0f;

                        if (eA.has<E_Mass>()) invMassA = eA.get<E_Mass>().invMass;
                        else if (cA.isStatic) invMassA = 0.0f;

                        if (eB.has<E_Mass>()) invMassB = eB.get<E_Mass>().invMass;
                        else if (cB.isStatic) invMassB = 0.0f;

                        if (invMassA == 0.0f && invMassB == 0.0f) return;

                        float totalInvMass = invMassA + invMassB;
                        if (totalInvMass <= 0.0f) return;

                        tA.x -= mtv.x * (invMassA / totalInvMass);
                        tA.y -= mtv.y * (invMassA / totalInvMass);

                        tB.x += mtv.x * (invMassB / totalInvMass);
                        tB.y += mtv.y * (invMassB / totalInvMass);

                        Vec2 n = normalize(mtv);
                        if (lenSq(n) < 1e-8f) return;

                        E_Velocity* vA = eA.has<E_Velocity>() ? &eA.get_mut<E_Velocity>() : nullptr;
                        E_Velocity* vB = eB.has<E_Velocity>() ? &eB.get_mut<E_Velocity>() : nullptr;

                        Vec2 velA = vA ? Vec2{vA->vx, vA->vy} : Vec2{0, 0};
                        Vec2 velB = vB ? Vec2{vB->vx, vB->vy} : Vec2{0, 0};

                        Vec2 relVel = sub(velB, velA);
                        float velAlongNormal = dot(relVel, n);

                        if (velAlongNormal > 0.0f) return;

                        float e = 0.3f;
                        if (eA.has<E_PhysicsMaterial>() && eB.has<E_PhysicsMaterial>()) {
                            float resA = eA.get<E_PhysicsMaterial>().restitution;
                            float resB = eB.get<E_PhysicsMaterial>().restitution;
                            e = (resA + resB) * 0.5f;
                        }

                        float j = -(1.0f + e) * velAlongNormal / totalInvMass;
                        Vec2 impulse = mul(n, j);

                        if (vA && invMassA > 0.0f) {
                            vA->vx -= impulse.x * invMassA;
                            vA->vy -= impulse.y * invMassA;
                        }
                        if (vB && invMassB > 0.0f) {
                            vB->vx += impulse.x * invMassB;
                            vB->vy += impulse.y * invMassB;
                        }
                    };

                    // Pass A: Huge bodies
                    for (flecs::entity_t idA : bigBodies_) {
                        if (!it.world().is_alive(idA)) continue;
                        flecs::entity eHuge = it.world().entity(idA);

                        if (eHuge.has<E_Transform>() && eHuge.has<E_Collider>()) {
                            E_Transform& tHuge = eHuge.get_mut<E_Transform>();
                            E_Collider&  cHuge = eHuge.get_mut<E_Collider>();
                            
                            if(!cHuge.active) continue;

                            qColliders_.each([&](flecs::entity eOther, E_Transform& tOther, E_Collider& cOther) {
                                if (eOther.id() == idA) return;
                                if (!tryMarkPair(idA, eOther.id())) return;
                                resolvePair(eHuge, tHuge, cHuge, eOther, tOther, cOther);
                            });
                        }
                    }

                    // Pass B: Grid
                    float cx = tA.x + cA.offsetX;
                    float cy = tA.y + cA.offsetY;
                    Vec2 half = aabbHalfExtents(cA);

                    Vec2 vel = {0,0};
                    if (eA.has<E_Velocity>()) {
                        E_Velocity& v = eA.get_mut<E_Velocity>();
                        vel = {v.vx, v.vy};
                    }
                    
                    float dt = it.delta_time();
                    float vx = vel.x * dt;
                    float vy = vel.y * dt;

                    float minX = cx - half.x + std::fmin(0.0f, vx);
                    float maxX = cx + half.x + std::fmax(0.0f, vx);
                    float minY = cy - half.y + std::fmin(0.0f, vy);
                    float maxY = cy + half.y + std::fmax(0.0f, vy);

                    int cellMinX = (int)floorf(minX / (float)CELL_SIZE);
                    int cellMaxX = (int)floorf(maxX / (float)CELL_SIZE);
                    int cellMinY = (int)floorf(minY / (float)CELL_SIZE);
                    int cellMaxY = (int)floorf(maxY / (float)CELL_SIZE);

                    for (int x = cellMinX; x <= cellMaxX; ++x)
                    for (int y = cellMinY; y <= cellMaxY; ++y) {
                        long long key = hashCellGlobal(x, y);
                        if (grid_.find(key) == grid_.end()) continue;

                        for (flecs::entity_t idB : grid_[key]) {
                            if (!tryMarkPair(eA.id(), idB)) continue;
                            if (!it.world().is_alive(idB)) continue;

                            flecs::entity eB = it.world().entity(idB);

                            if (eB.has<E_Transform>() && eB.has<E_Collider>()) {
                                E_Transform& tB = eB.get_mut<E_Transform>();
                                E_Collider&  cB = eB.get_mut<E_Collider>();
                                resolvePair(eA, tA, cA, eB, tB, cB);
                            }
                        }
                    }
                }
            }
        });
    
    // --- Camera System ---
    world.system<E_Transform, E_Camera>("CameraSystem")
        .each([](flecs::entity e, E_Transform& t, E_Camera& cam) {
            if (!cam.active) return;

            GLFWwindow* w = glfwGetCurrentContext();
            if (!w) return;

            int winW = 0, winH = 0;
            glfwGetWindowSize(w, &winW, &winH);

            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();

            glTranslatef(winW * 0.5f, winH * 0.5f, 0.f);
            glScalef(cam.zoom, cam.zoom, 1.f);
            glTranslatef(-winW * 0.5f, -winH * 0.5f, 0.f);
            glTranslatef(-t.x + winW * 0.5f, -t.y + winH * 0.5f, 0.f);
        });

    // --- Clickable System ---
    world.system<E_Transform, E_Sprite, E_Clickable>("ClickableSystem")
        .each([this](flecs::entity e, E_Transform& t, E_Sprite& s, E_Clickable& btn){
            auto input = e.world().get_ref<E_InputState>();

            bool hover = hoverIt(s,e,t); 

            btn.isHovered = hover;
            btn.isClicked = hover && input->leftPressed;

            if (btn.isClicked && btn.onClick) {
                btn.onClick();
            }
        });
    
    // --- Post Update: Clear Events ---
    world.system<E_CollisionEvent>("ClearCollisionEvents")
        .kind(flecs::PostUpdate)
        .each([](flecs::entity e, E_CollisionEvent&) {
            e.destruct();
        });

    printf("[Engine] ECS world init done\n");
}
void ECSWorld::initRender() {
    printf("[Engine] Initializing Render Resources...\n");
    
    const char* vShaderSrc = R"(
        #version 330 core
        layout (location = 0) in vec2 aPos;
        layout (location = 1) in vec2 aTexCoord;

        out vec2 TexCoord;

        uniform mat4 model;
        uniform mat4 view;
        uniform mat4 projection;

        void main() {
            gl_Position = projection * view * model * vec4(aPos, 0.0, 1.0);
            TexCoord = aTexCoord;
        }
    )";

    const char* fShaderSrc = R"(
        #version 330 core
        out vec4 FragColor;
        
        in vec2 TexCoord;

        uniform vec4 color;
        uniform sampler2D image;
        uniform bool useTexture;

        void main() {
            if(useTexture) {
                vec4 texColor = texture(image, TexCoord);
                FragColor = texColor * color;
            } else {
                FragColor = color;
            }
        }
    )";

    shaderProgram = createShader(vShaderSrc, fShaderSrc);
    uModelLoc = glGetUniformLocation(shaderProgram, "model");
    uViewLoc = glGetUniformLocation(shaderProgram, "view");
    uProjLoc = glGetUniformLocation(shaderProgram, "projection");
    uColorLoc = glGetUniformLocation(shaderProgram, "color");
    uUseTexLoc = glGetUniformLocation(shaderProgram, "useTexture");
    uTextureLoc = glGetUniformLocation(shaderProgram, "image");
    uBloomLoc = glGetUniformLocation(shaderProgram, "bloomIntensity");

      // --- 1. RECTANGLE (Quad) ---
    float rectVerts[] = { 
        // pos        // tex
        -0.5f, -0.5f,  0.0f, 0.0f, 
         0.5f, -0.5f,  1.0f, 0.0f, 
         0.5f,  0.5f,  1.0f, 1.0f, 
        -0.5f,  0.5f,  0.0f, 1.0f  
    };
    unsigned int rectInd[] = { 0, 1, 2, 2, 3, 0 };

    glGenVertexArrays(1, &VAO_Rect); glGenBuffers(1, &VBO_Rect); 
    unsigned int EBO_Rect; glGenBuffers(1, &EBO_Rect);

    glBindVertexArray(VAO_Rect);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_Rect);
    glBufferData(GL_ARRAY_BUFFER, sizeof(rectVerts), rectVerts, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO_Rect);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(rectInd), rectInd, GL_STATIC_DRAW);
    
    // Attribs (0=Pos, 1=Tex)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)(2*sizeof(float)));
    glEnableVertexAttribArray(1);

    // --- 2. TRIANGLE ---
    float triVerts[] = {
        // pos         // tex (примерные)
        -0.5f,  0.5f,  0.0f, 1.0f, // Left Bottom
         0.5f,  0.5f,  1.0f, 1.0f, // Right Bottom
         0.0f, -0.5f,  0.5f, 0.0f  // Top Center
    };

    glGenVertexArrays(1, &VAO_Tri); glGenBuffers(1, &VBO_Tri);
    glBindVertexArray(VAO_Tri);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_Tri);
    glBufferData(GL_ARRAY_BUFFER, sizeof(triVerts), triVerts, GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)(2*sizeof(float)));
    glEnableVertexAttribArray(1);

    // --- 3. CIRCLE ---
    std::vector<float> circVerts;
    // Center
    circVerts.push_back(0.0f); circVerts.push_back(0.0f); // Pos
    circVerts.push_back(0.5f); circVerts.push_back(0.5f); // Tex
    
    for(int i=0; i<=numCircleVerts; i++) {
        float a = (float)i / (float)numCircleVerts * 6.283185f;
        float x = cosf(a) * 0.5f; // Radius 0.5 (diam 1.0)
        float y = sinf(a) * 0.5f;
        circVerts.push_back(x); circVerts.push_back(y);
        circVerts.push_back(x + 0.5f); circVerts.push_back(y + 0.5f);
    }
    
    glGenVertexArrays(1, &VAO_Circle); glGenBuffers(1, &VBO_Circle);
    glBindVertexArray(VAO_Circle);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_Circle);
    glBufferData(GL_ARRAY_BUFFER, circVerts.size()*sizeof(float), circVerts.data(), GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)(2*sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    world.system<E_Transform, E_Sprite>("RenderSystem")
        .each([this]
              (flecs::entity e, E_Transform& t, E_Sprite& sprite) {
            
            const E_Color* col = e.has<E_Color>() ? &e.get<E_Color>() : nullptr;
            const E_Texture* tex = e.has<E_Texture>() ? &e.get<E_Texture>() : nullptr;
            const E_EffectBloom* bloom = e.has<E_EffectBloom>() ? &e.get<E_EffectBloom>() : nullptr;
            const E_EffectHover* hover = e.has<E_EffectHover>() ? &e.get<E_EffectHover>() : nullptr;
            const E_EffectShadow* shadow = e.has<E_EffectShadow>() ? &e.get<E_EffectShadow>() : nullptr;
            const E_EffectOutline* outline = e.has<E_EffectOutline>() ? &e.get<E_EffectOutline>() : nullptr;
            const E_EffectTranspare* trans = e.has<E_EffectTranspare>() ? &e.get<E_EffectTranspare>() : nullptr;

            if(hover) hoverIt(sprite, e, t);

            float alpha = (trans && trans->work) ? trans->alpha : 1.0f;

            glUseProgram(shaderProgram);
            int winW = 800, winH = 600;
            const E_WindowSize* ws = e.world().has<E_WindowSize>() ? &e.world().get<E_WindowSize>() : nullptr;
            if (ws) { winW = ws->w; winH = ws->h; }

            glm::mat4 projection = glm::ortho(0.0f, (float)winW, (float)winH, 0.0f, -100.0f, 100.0f);
            glUniformMatrix4fv(uProjLoc, 1, GL_FALSE, glm::value_ptr(projection));

            glm::mat4 view = glm::mat4(1.0f);

            float camX = winW * 0.5f;
            float camY = winH * 0.5f;
            float zoom = 1.0f;
            bool camFound = false;

            qCamera_.each([&](flecs::entity eCam, E_Transform& ct, E_Camera& cc) {
                if (cc.active) {
                    camX = ct.x;
                    camY = ct.y;
                    zoom = cc.zoom;
                    camFound = true;
                }
            });

            if (camFound) {
                view = glm::translate(view, glm::vec3(winW * 0.5f, winH * 0.5f, 0.0f));
                view = glm::scale(view, glm::vec3(zoom, zoom, 1.0f));
                view = glm::translate(view, glm::vec3(-camX, -camY, 0.0f));
            }

            glUniformMatrix4fv(uViewLoc, 1, GL_FALSE, glm::value_ptr(view));
            // =========================================================
            
                        auto drawPass = [&](glm::vec3 posOffset, glm::vec3 scale, glm::vec4 color, bool useTex, float bloomVal) {
                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, glm::vec3(t.x + posOffset.x, t.y + posOffset.y, t.layer + posOffset.z)); 
                model = glm::rotate(model, glm::radians(t.angle), glm::vec3(0.0f, 0.0f, 1.0f)); 
                
                unsigned int currentVAO = VAO_Rect;
                int count = 6;
                GLenum mode = GL_TRIANGLES;
                bool indexed = true;

                float sx = sprite.width;
                float sy = sprite.height;

                if (sprite.type == E_Sprite::CIRCLE) {
                    currentVAO = VAO_Circle;
                    sx = sprite.radius * 2.0f;
                    sy = sprite.radius * 2.0f;
                    count = numCircleVerts + 2; // Center + 32 points + 1 closing
                    mode = GL_TRIANGLE_FAN;
                    indexed = false;
                } else if (sprite.type == E_Sprite::TRIANGLE) {
                    currentVAO = VAO_Tri;
                    count = 3;
                    mode = GL_TRIANGLES;
                    indexed = false;
                }

                model = glm::scale(model, glm::vec3(sx * t.xScale * scale.x, sy * t.yScale * scale.y, 1.0f)); 
                
                glUniformMatrix4fv(uModelLoc, 1, GL_FALSE, glm::value_ptr(model));
                glUniform4f(uColorLoc, color.r, color.g, color.b, color.a * alpha);
                glUniform1f(uBloomLoc, bloomVal);
                glUniform1i(uUseTexLoc, useTex);

                glBindVertexArray(currentVAO);
                if (indexed) {
                    glDrawElements(mode, count, GL_UNSIGNED_INT, 0);
                } else {
                    glDrawArrays(mode, 0, count);
                }
            };


            // 1. Shadow
            if (shadow && shadow->work) {
                drawPass(glm::vec3(shadow->offset, shadow->offset, -0.1f), 
                         glm::vec3(1.0f), glm::vec4(0,0,0,0.5f), false, 0.0f);
            }

            // 2. Outline
            if (outline && outline->work) {
                float scaleFactor = 1.05f; // +5% размера
                drawPass(glm::vec3(0,0,-0.05f), glm::vec3(scaleFactor), 
                         glm::vec4(0,0,0,1), false, 0.0f);
            }

            // 3. Main Sprite
            float r=1, g=1, b=1;
            if(col) { r=col->r; g=col->g; b=col->b; }
            float bloomVal = (bloom && bloom->work) ? bloom->intensity : 0.0f;

            if (tex && tex->id > 0) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, tex->id);
                glUniform1i(uTextureLoc, 0);
                drawPass(glm::vec3(0), glm::vec3(1), glm::vec4(r,g,b,1), true, bloomVal);
            } else {
                drawPass(glm::vec3(0), glm::vec3(1), glm::vec4(r,g,b,1), false, bloomVal);
            }
            
            glBindVertexArray(0);
        });

    printf("[Engine] Render Resources initialized.\n");
}



void ECSWorld::update(float dt) {
    world.progress(dt);
}

bool ECSWorld::hoverIt(E_Sprite &s, flecs::entity &e, E_Transform &t)
{
    // 1. Check if entity is valid
    if (!e.is_alive()) return false;

    // 2. Get World and Check Input Presence
    const flecs::world& w = e.world();
    if (!w.has<E_InputState>()) return false;

    // 3. Safe Input Access (Take address of reference)
    const E_InputState* input = &w.get<E_InputState>();

    float mx = (float)input->mouseX;
    float my = (float)input->mouseY;

    // 4. Safe Window Size Access
    float winW = 800.0f;
    float winH = 600.0f;
    
    if (w.has<E_WindowSize>()) {
        const E_WindowSize* ws = &w.get<E_WindowSize>(); // Address of reference
        winW = (float)ws->w;
        winH = (float)ws->h;
    }

    // 5. Camera Logic
    float camX = winW * 0.5f; 
    float camY = winH * 0.5f;
    float zoom = 1.0f;

    qCamera_.each([&](flecs::entity eCam, E_Transform& ct, E_Camera& cc) {
        if (cc.active) {
            camX = ct.x;
            camY = ct.y;
            zoom = cc.zoom;
        }
    });

    // 6. Coordinates Calculation
    float screenCenteredX = mx - winW * 0.5f;
    float screenCenteredY = my - winH * 0.5f;

    float unzoomedX = screenCenteredX / zoom;
    float unzoomedY = screenCenteredY / zoom;

    float worldMX = unzoomedX + camX;
    float worldMY = unzoomedY + camY;

    // 7. AABB Logic
    float left, right, top, bottom;

    switch(s.type) {
        case E_Sprite::RECTANGLE: {
            float hw = s.width / 2.0f;
            float hh = s.height / 2.0f;
            left = t.x - hw;
            right = t.x + hw;
            top = t.y - hh;
            bottom = t.y + hh;
            break;
        }
        case E_Sprite::CIRCLE: {
            left = t.x - s.radius;
            right = t.x + s.radius;
            top = t.y - s.radius;
            bottom = t.y + s.radius;
            break;
        }
        case E_Sprite::TRIANGLE: {
            float hw = s.width / 2.0f;
            float hh = s.height / 2.0f;
            left = t.x - hw;
            right = t.x + hw;
            top = t.y - hh;
            bottom = t.y + hh;
            break;
        }
        default: return false;
    }

    bool isInside = (worldMX >= left && worldMX <= right && worldMY >= top && worldMY <= bottom);

    // 8. Safe Effect Access
    if (e.has<E_EffectHover>()) {
        const E_EffectHover* Chover = &e.get<E_EffectHover>(); // Address of reference
        if (Chover->work) {
             if (isInside) {
                t.xScale = Chover->offsetX; 
                t.yScale = Chover->offsetY;
            } else {
                t.xScale = 1.0f; 
                t.yScale = 1.0f;
            }
        }
    }

    return isInside;
}

// === Shaders ===

unsigned int ECSWorld::createShader(const char* vertexSource, const char* fragmentSource) {
    unsigned int vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vertexSource, NULL);
    glCompileShader(vs);
    // TODO check errors

    unsigned int fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fragmentSource, NULL);
    glCompileShader(fs);

    unsigned int prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}
