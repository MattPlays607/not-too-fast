
#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/CCScheduler.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/loader/GameEvent.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>
#include <fmod.hpp>
#include <algorithm>
#include <string>

using namespace geode::prelude;

static float g_speedMultiplier = 1.0f;
static float g_savedLevelSpeed = 1.0f;
static bool g_deathCounted = false;

static constexpr int SPEED_LABEL_TAG = 60701;

$on_mod(Loaded) {
    float startingSpeed =
        Mod::get()->getSettingValue<float>("starting-speed");

    g_speedMultiplier = startingSpeed;
    g_savedLevelSpeed = startingSpeed;
}

$on_game(Loaded) {
    listenForKeybindSettingPresses(
        "reset-key",
        [](Keybind const& keybind, bool down, bool repeat, double timestamp) {
            if (down && !repeat) {
                float startingSpeed =
                    Mod::get()->getSettingValue<float>("starting-speed");

                g_speedMultiplier = startingSpeed;
                g_savedLevelSpeed = startingSpeed;
            }
        }
    );
}

static void increaseSpeed(const char* event) {
    auto trigger = Mod::get()->getSettingValue<std::string>("trigger");

    if (trigger != std::string(event))
        return;

    float increase =
        Mod::get()->getSettingValue<float>("speed-increase");

    float maxSpeed = static_cast<float>(
        Mod::get()->getSettingValue<int64_t>("max-speed")
    );

    g_speedMultiplier = std::min(
        g_speedMultiplier + (increase / 100.0f),
        maxSpeed
    );
}

class $modify(NotTooFastScheduler, cocos2d::CCScheduler) {
    void update(float dt) override {
        dt *= g_speedMultiplier;
        CCScheduler::update(dt);
    }
};

class $modify(NotTooFastAudio, FMODAudioEngine) {
    void update(float dt) {
        FMODAudioEngine::update(dt);

        if (!m_system)
            return;

        FMOD::ChannelGroup* masterGroup = nullptr;

        if (m_system->getMasterChannelGroup(&masterGroup) == FMOD_OK) {
            if (masterGroup) {
                masterGroup->setPitch(g_speedMultiplier);
            }
        }
    }
};

class $modify(NotTooFastGameLayer, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool isPlayer1) {
        GJBaseGameLayer::handleButton(down, button, isPlayer1);

        bool isJump =
            down && button == static_cast<int>(PlayerButton::Jump);

        if (isJump) {
            increaseSpeed("click");
        }
    }

    void update(float dt) override {
        GJBaseGameLayer::update(dt);

        auto label = this->getChildByTag(SPEED_LABEL_TAG);

        if (!label)
            return;

        bool showSpeed =
            Mod::get()->getSettingValue<bool>("show-speed");

        label->setVisible(showSpeed);

        if (showSpeed) {
            auto text = fmt::format("x{:.2f}", g_speedMultiplier);
            static_cast<CCLabelBMFont*>(label)->setString(text.c_str());
        }
    }
};

class $modify(NotTooFastPlayer, PlayerObject) {
    void playerDestroyed(bool isPlayer1) {
        PlayerObject::playerDestroyed(isPlayer1);

        if (!g_deathCounted) {
            g_deathCounted = true;
            increaseSpeed("death");
        }
    }
};

class $modify(NotTooFastPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool dontCreateObjects) {
        bool keepSpeed =
            Mod::get()->getSettingValue<bool>("keep-speed-between-levels");

        if (keepSpeed) {
            g_speedMultiplier = g_savedLevelSpeed;
        } else {
            g_speedMultiplier =
                Mod::get()->getSettingValue<float>("starting-speed");

            g_savedLevelSpeed = g_speedMultiplier;
        }

        g_deathCounted = false;

        if (!PlayLayer::init(level, dontCreateObjects))
            return false;

        auto winSize = CCDirector::get()->getWinSize();

        auto label = CCLabelBMFont::create("x1.00", "bigFont.fnt");

        if (label) {
            label->setScale(0.5f);
            label->setAnchorPoint({1.0f, 1.0f});
            label->setPosition({
                winSize.width - 12.0f,
                winSize.height - 12.0f
            });
            label->setZOrder(1000);
            label->setTag(SPEED_LABEL_TAG);

            this->addChild(label);
        }

        return true;
    }

    void startGame() {
        g_deathCounted = false;
        PlayLayer::startGame();
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        g_deathCounted = false;
    }

    void onQuit() {
        bool keepSpeed =
            Mod::get()->getSettingValue<bool>("keep-speed-between-levels");

        if (keepSpeed) {
            g_savedLevelSpeed = g_speedMultiplier;
        } else {
            g_savedLevelSpeed =
                Mod::get()->getSettingValue<float>("starting-speed");
        }

        g_speedMultiplier = 1.0f;
        g_deathCounted = false;

        PlayLayer::onQuit();
    }
};
