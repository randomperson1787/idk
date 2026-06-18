#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/CCScheduler.hpp>

using namespace geode::prelude;

static cocos2d::CCPoint s_prevPos1 = {0, 0};
static cocos2d::CCPoint s_currPos1 = {0, 0};
static float s_prevRot1 = 0.f;
static float s_currRot1 = 0.f;

static cocos2d::CCPoint s_prevPos2 = {0, 0};
static cocos2d::CCPoint s_currPos2 = {0, 0};
static float s_prevRot2 = 0.f;
static float s_currRot2 = 0.f;

static bool s_initialized1 = false;
static bool s_initialized2 = false;
static bool s_inLevel = false;
static float s_accumulator = 0.f;
static const float PHYSICS_RATE = 1.f / 240.f;

class $modify(VTPSPlayLayer, PlayLayer) {
    void update(float dt) {
        PlayLayer::update(dt);

        if (!m_player1) return;

        if (!s_initialized1) {
            s_prevPos1 = m_player1->getPosition();
            s_currPos1 = m_player1->getPosition();
            s_prevRot1 = m_player1->getRotation();
            s_currRot1 = m_player1->getRotation();
            s_initialized1 = true;
        } else {
            s_prevPos1 = s_currPos1;
            s_prevRot1 = s_currRot1;
            s_currPos1 = m_player1->getPosition();
            s_currRot1 = m_player1->getRotation();
        }

        if (m_player2) {
            if (!s_initialized2) {
                s_prevPos2 = m_player2->getPosition();
                s_currPos2 = m_player2->getPosition();
                s_prevRot2 = m_player2->getRotation();
                s_currRot2 = m_player2->getRotation();
                s_initialized2 = true;
            } else {
                s_prevPos2 = s_currPos2;
                s_prevRot2 = s_currRot2;
                s_currPos2 = m_player2->getPosition();
                s_currRot2 = m_player2->getRotation();
            }
        }
    }

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        s_initialized1 = false;
        s_initialized2 = false;
        s_accumulator = 0.f;
        s_inLevel = true;
        return true;
    }

    void onQuit() {
        s_inLevel = false;
        s_initialized1 = false;
        s_initialized2 = false;
        PlayLayer::onQuit();
    }

    void visit() {
        if (!s_inLevel || !m_player1) {
            PlayLayer::visit();
            return;
        }

        auto* mod = Mod::get();
        if (!mod->getSettingValue<bool>("enabled")) {
            PlayLayer::visit();
            return;
        }

        int64_t visualTPS = mod->getSettingValue<int64_t>("visual-tps");
        if (visualTPS <= 0) visualTPS = 240;

        float alpha = s_accumulator / PHYSICS_RATE;
        if (alpha > 1.f) alpha = 1.f;

        float visualInterval = 1.f / static_cast<float>(visualTPS);
        float snapped = (float)((int)(alpha / (visualInterval / PHYSICS_RATE))) * (visualInterval / PHYSICS_RATE);
        if (snapped > 1.f) snapped = 1.f;

        // Save real positions
        auto realPos1 = m_player1->getPosition();
        auto realRot1 = m_player1->getRotation();

        // Apply interpolated position
        cocos2d::CCPoint interpPos1 = {
            s_prevPos1.x + (s_currPos1.x - s_prevPos1.x) * snapped,
            s_prevPos1.y + (s_currPos1.y - s_prevPos1.y) * snapped
        };
        float interpRot1 = s_prevRot1 + (s_currRot1 - s_prevRot1) * snapped;

        m_player1->setPosition(interpPos1);
        m_player1->setRotation(interpRot1);

        if (m_player2 && s_initialized2) {
            auto realPos2 = m_player2->getPosition();
            auto realRot2 = m_player2->getRotation();

            cocos2d::CCPoint interpPos2 = {
                s_prevPos2.x + (s_currPos2.x - s_prevPos2.x) * snapped,
                s_prevPos2.y + (s_currPos2.y - s_prevPos2.y) * snapped
            };
            float interpRot2 = s_prevRot2 + (s_currRot2 - s_prevRot2) * snapped;

            m_player2->setPosition(interpPos2);
            m_player2->setRotation(interpRot2);

            PlayLayer::visit();

            m_player2->setPosition(realPos2);
            m_player2->setRotation(realRot2);
        } else {
            PlayLayer::visit();
        }

        // Restore real positions
        m_player1->setPosition(realPos1);
        m_player1->setRotation(realRot1);
    }
};

class $modify(VTPSScheduler, cocos2d::CCScheduler) {
    void update(float dt) {
        if (s_inLevel) {
            s_accumulator += dt;
            while (s_accumulator >= PHYSICS_RATE) {
                s_accumulator -= PHYSICS_RATE;
            }
        }
        cocos2d::CCScheduler::update(dt);
    }
};
