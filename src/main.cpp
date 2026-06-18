#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>

using namespace geode::prelude;

// Stores the previous and current physics positions for interpolation
struct PlayerInterp {
    cocos2d::CCPoint prevPos;
    cocos2d::CCPoint currPos;
    float prevRotation;
    float currRotation;
    bool initialized = false;
};

static PlayerInterp s_player1;
static PlayerInterp s_player2;

// Time accumulator for visual TPS ticks
static float s_accumulator = 0.f;
static float s_lastPhysicsTime = 0.f;
static bool s_inLevel = false;

class $modify(VisualTPSPlayLayer, PlayLayer) {

    // Called every physics update (real TPS)
    void updateVisualPositions(PlayerObject* player, PlayerInterp& interp) {
        if (!player) return;

        if (!interp.initialized) {
            interp.prevPos = player->getPosition();
            interp.currPos = player->getPosition();
            interp.prevRotation = player->getRotation();
            interp.currRotation = player->getRotation();
            interp.initialized = true;
            return;
        }

        // Shift current -> previous, then store new current
        interp.prevPos = interp.currPos;
        interp.prevRotation = interp.currRotation;
        interp.currPos = player->getPosition();
        interp.currRotation = player->getRotation();
    }

    void update(float dt) {
        // Run the real game update (physics untouched)
        PlayLayer::update(dt);

        if (!Mod::get()->getSettingValue<bool>("enabled")) return;
        if (!m_player1) return;

        // Capture positions after real physics update
        updateVisualPositions(m_player1, s_player1);
        if (m_player2) updateVisualPositions(m_player2, s_player2);
    }

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        // Reset interpolation state on level load
        s_player1 = PlayerInterp();
        s_player2 = PlayerInterp();
        s_accumulator = 0.f;
        s_inLevel = true;

        return true;
    }

    void onQuit() {
        s_inLevel = false;
        s_player1 = PlayerInterp();
        s_player2 = PlayerInterp();
        PlayLayer::onQuit();
    }
};

// Hook the draw/visit cycle to interpolate icon position every frame
class $modify(VisualTPSPlayerObject, PlayerObject) {

    void draw() {
        // Only interpolate if we're in a level and the mod is enabled
        if (!s_inLevel || !Mod::get()->getSettingValue<bool>("enabled")) {
            PlayerObject::draw();
            return;
        }

        auto* pl = PlayLayer::get();
        if (!pl) {
            PlayerObject::draw();
            return;
        }

        // Figure out which player this is
        PlayerInterp* interp = nullptr;
        if (this == pl->m_player1) interp = &s_player1;
        else if (this == pl->m_player2) interp = &s_player2;

        if (!interp || !interp->initialized) {
            PlayerObject::draw();
            return;
        }

        // Get visual TPS from settings
        int visualTPS = Mod::get()->getSettingValue<int64_t>("visual-tps");
        if (visualTPS <= 0) visualTPS = 240;

        // Calculate interpolation alpha based on time within current physics tick
        // We use the director's delta time to figure out how far between ticks we are
        float physicsInterval = 1.f / 240.f; // GD default physics rate
        float alpha = std::clamp(s_accumulator / physicsInterval, 0.f, 1.f);

        // For very high visual TPS, alpha approaches 1 (always show latest position)
        // For low visual TPS, we snap to intervals
        float visualInterval = 1.f / static_cast<float>(visualTPS);
        float snappedAlpha = std::floor(alpha / visualInterval) * visualInterval;
        snappedAlpha = std::clamp(snappedAlpha / physicsInterval, 0.f, 1.f);

        // Lerp position
        cocos2d::CCPoint interpPos = {
            interp->prevPos.x + (interp->currPos.x - interp->prevPos.x) * snappedAlpha,
            interp->prevPos.y + (interp->currPos.y - interp->prevPos.y) * snappedAlpha
        };

        // Lerp rotation (simple lerp, works for most cases)
        float interpRot = interp->prevRotation + (interp->currRotation - interp->prevRotation) * snappedAlpha;

        // Save real position/rotation
        auto realPos = this->getPosition();
        auto realRot = this->getRotation();

        // Apply interpolated visual position
        this->setPosition(interpPos);
        this->setRotation(interpRot);

        PlayerObject::draw();

        // Restore real position so physics are unaffected
        this->setPosition(realPos);
        this->setRotation(realRot);
    }
};

// Track frame delta for interpolation alpha
#include <Geode/modify/CCScheduler.hpp>

class $modify(VisualTPSScheduler, cocos2d::CCScheduler) {
    void update(float dt) {
        if (s_inLevel) {
            float physicsInterval = 1.f / 240.f;
            s_accumulator += dt;
            // Reset accumulator each physics tick
            while (s_accumulator >= physicsInterval) {
                s_accumulator -= physicsInterval;
            }
        }
        cocos2d::CCScheduler::update(dt);
    }
};
