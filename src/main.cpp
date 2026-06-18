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
};

class $modify(VTPSPlayerObject, PlayerObject) {
    void draw() {
        if (!s_inLevel) {
            PlayerObject::draw();
            return;
        }

        auto* mod = Mod::get();
        if (!mod->getSettingValue<bool>("enabled")) {
            PlayerObject::draw();
            return;
        }

        auto* pl = PlayLayer::get();
        if (!pl) {
            PlayerObject::draw();
            return;
        }

        bool isP1 = (this == pl->m_player1);
        bool isP2 = (this == pl->m_player2);

        if ((!isP1 && !isP2)) {
            PlayerObject::draw();
            return;
        }

        bool initialized = isP1 ? s_initialized1 : s_initialized2;
        if (!initialized) {
            PlayerObject::draw();
            return;
        }

        int64_t visualTPS = mod->getSettingValue<int64_t>("visual-tps");
        if (visualTPS <= 0) visualTPS = 240;

        float alpha = s_accumulator / PHYSICS_RATE;
        if (alpha > 1.f) alpha = 1.f;

        float visualInterval = 1.f / static_cast<float>(visualTPS);
        float snapped = (float)((int)(alpha / (visualInterval / PHYSICS_RATE))) * (visualInterval / PHYSICS_RATE);
        if (snapped > 1.f) snapped = 1.f;

        cocos2d::CCPoint prev = isP1 ? s_prevPos1 : s_prevPos2;
        cocos2d::CCPoint curr = isP1 ? s_currPos1 : s_currPos2;
        float prevR = isP1 ? s_prevRot1 : s_prevRot2;
        float currR = isP1 ? s_currRot1 : s_currRot2;

        cocos2d::CCPoint interpPos = {
            prev.x + (curr.x - prev.x) * snapped,
            prev.y + (curr.y - prev.y) * snapped
        };
        float interpRot = prevR + (currR - prevR) * snapped;

        auto realPos = this->getPosition();
        auto realRot = this->getRotation();

        this->setPosition(interpPos);
        this->setRotation(interpRot);
        PlayerObject::draw();
        this->setPosition(realPos);
        this->setRotation(realRot);
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
