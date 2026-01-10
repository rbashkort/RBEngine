// 
//  ecs_world.cpp rbashkort 01/01/2026
//

#include "ecs_world.h"
#include "components.h"
#include "physics.hpp"

#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <flecs/addons/cpp/entity.hpp>
#include <flecs/addons/cpp/iter.hpp>
#include <flecs/addons/cpp/mixins/pipeline/decl.hpp>
#include <flecs/addons/cpp/ref.hpp>
#include <math.h>
#include <cstdio>

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

    qColliders_ = world.query<E_Transform, E_Collider>();

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

    // --- Render System ---
    world.system<E_Transform, E_Sprite>("RenderSystem")
        .each([this](flecs::entity e, E_Transform& t, E_Sprite& sprite) {
            const E_Color* color = e.has<E_Color>() ? &e.get<E_Color>() : nullptr;
            const E_Texture* tex = e.has<E_Texture>() ? &e.get<E_Texture>() : nullptr;
            const E_EffectShadow* shadow = e.has<E_EffectShadow>() ? &e.get<E_EffectShadow>() : nullptr;
            const E_EffectOutline* outline = e.has<E_EffectOutline>() ? &e.get<E_EffectOutline>() : nullptr;
            const E_EffectTranspare* trans = e.has<E_EffectTranspare>() ? &e.get<E_EffectTranspare>() : nullptr;
            const E_EffectHover* hover = e.has<E_EffectHover>() ? &e.get<E_EffectHover>() : nullptr;

            if(hover) {
                hoverIt(sprite, e, t);
            }

            glPushMatrix();
            glTranslatef(t.x, t.y, t.layer);
            glRotatef(t.angle, 0.f, 0.f, 1.f);
            glScalef(t.xScale, t.yScale, 1.f);

            float alpha = (trans && trans->work) ? trans->alpha : 1.f;

            if (shadow && shadow->work) {
                glPushMatrix();
                glTranslatef(shadow->offset, shadow->offset, t.layer-1.f);
                glDisable(GL_TEXTURE_2D);
                glColor4f(0.f, 0.f, 0.f, 0.5f * alpha);
                drawSprite(sprite);
                glPopMatrix();
            }

            if (tex && tex->id != 0) {
                glColor4f(1.f, 1.f, 1.f, alpha); 
                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, tex->id);

                float hw = sprite.width / 2.0f;
                float hh = sprite.height / 2.0f;

                glBegin(GL_QUADS);
                glTexCoord2f(0.f, 0.f); glVertex2f(-hw, -hh);
                glTexCoord2f(1.f, 0.f); glVertex2f( hw, -hh);
                glTexCoord2f(1.f, 1.f); glVertex2f( hw,  hh);
                glTexCoord2f(0.f, 1.f); glVertex2f(-hw,  hh);
                glEnd();
    
                glBindTexture(GL_TEXTURE_2D, 0);
                glDisable(GL_TEXTURE_2D);
            }
            else {
                if (color) glColor4f(color->r, color->g, color->b, alpha);
                else glColor4f(1.f, 1.f, 1.f, alpha);
                drawSprite(sprite);
            }

            if(outline && outline->work) {
                glLineWidth((float)outline->length);
                glColor4f(0.f, 0.f, 0.f, alpha);
                drawSprite(sprite, true);
            }

            glPopMatrix();
        });
    
    // --- Post Update: Clear Events ---
    world.system<E_CollisionEvent>("ClearCollisionEvents")
        .kind(flecs::PostUpdate)
        .each([](flecs::entity e, E_CollisionEvent&) {
            e.destruct();
        });

    printf("[Engine] ECS world init done\n");
}


void ECSWorld::update(float dt) {
    world.progress(dt);
}

void ECSWorld::drawSprite(E_Sprite sprite, bool isLineLoop) {
    GLenum mode = isLineLoop ? GL_LINE_LOOP : (sprite.type == E_Sprite::RECTANGLE ? GL_QUADS : GL_TRIANGLE_FAN);

    if (!isLineLoop && sprite.type == E_Sprite::TRIANGLE) mode = GL_TRIANGLES;

    glBegin(mode);

    switch (sprite.type) {
        case E_Sprite::CIRCLE: {
            if(!isLineLoop) glVertex2f(0.f, 0.f); 
            for (int i = 0; i <= 32; ++i) {
                float a = 2.f * 3.14159f * i / 32.f;
                glVertex2f(cosf(a) * sprite.radius, sinf(a) * sprite.radius);
            }
            break;
        }
        case E_Sprite::RECTANGLE: {
            float hw = sprite.width / 2.0f;
            float hh = sprite.height / 2.0f;
            glVertex2f(-hw, -hh);
            glVertex2f( hw, -hh);
            glVertex2f( hw,  hh);
            glVertex2f(-hw,  hh);
            break;
        }
        case E_Sprite::TRIANGLE: {
            float hw = sprite.width / 2.0f;
            float hh = sprite.height / 2.0f;
            glVertex2f(-hw,  hh); // Left Bottom
            glVertex2f( hw,  hh); // Right Bottom
            glVertex2f( 0.f, -hh); // Top Center
            break;
        }
    }
    glEnd();
}

bool ECSWorld::hoverIt(E_Sprite &s, flecs::entity &e, E_Transform &t)
{
    auto input = e.world().get_ref<E_InputState>();
    float mx = (float)input->mouseX;
    float my = (float)input->mouseY;

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

    bool isInside = (mx >= left && mx <= right && my >= top && my <= bottom);

    const E_EffectHover* Chover = e.has<E_EffectHover>() ? &e.get<E_EffectHover>() : nullptr;
    if (Chover) {
         if (isInside) {
            t.xScale = Chover->offsetX; t.yScale = Chover->offsetY;
        } else {
            t.xScale = 1.0f; t.yScale = 1.0f;
        }
    }

    return isInside;
}

