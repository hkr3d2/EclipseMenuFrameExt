#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/binding/GJGroundLayer.hpp>
#include <Geode/binding/PlayerObject.hpp>

using namespace geode::prelude;

// Simple Frame Extrapolation manager class
class FrameExtrapolationManager {
public:
    static FrameExtrapolationManager* get() {
        static FrameExtrapolationManager instance;
        return &instance;
    }
    
    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled) { m_enabled = enabled; }
    
    std::string getName() const { return "Frame Extrapolation"; }
    std::string getDescription() const { 
        return "Smooths between frames by predicting where the player will be the next frame using its velocity."; 
    }
    
private:
    bool m_enabled = false;
};

class $modify(ExtrapolatedGameLayer, GJBaseGameLayer) {
    struct Fields {
        float timeTilNextTick = 0;
        float progressTilNextTick = 0;
        
        CCPoint lastCamPos2;
        CCPoint lastCamPos;
        float modifiedDeltaReturn = 0;
    };

    float getModifiedDelta(float dt) {
        auto pRet = GJBaseGameLayer::getModifiedDelta(dt);
        m_fields->modifiedDeltaReturn = pRet;
        return pRet;
    }

    virtual void update(float dt) {
        GJBaseGameLayer::update(dt);

        if (!typeinfo_cast<PlayLayer*>(this))
            return;

        if (!FrameExtrapolationManager::get()->isEnabled()) {
            return;
        }

        auto self = m_fields;

        if (this->isRunning() && dt != 0 && !PlayLayer::get()->m_levelEndAnimationStarted) {
            if (self->modifiedDeltaReturn != 0) {
                self->timeTilNextTick = self->modifiedDeltaReturn;
                self->progressTilNextTick = 0;
                self->lastCamPos2 = self->lastCamPos;
                self->lastCamPos = m_objectLayer->getPosition();
            } else {
                self->progressTilNextTick += dt;
            }

            if (self->timeTilNextTick == 0)
                return;

            // the percentage towards the next tick we are
            float percent = self->progressTilNextTick / self->timeTilNextTick;
            auto endCamPos = self->lastCamPos + (self->lastCamPos - self->lastCamPos2);
            
            m_objectLayer->setPosition(
                std::lerp<double>(self->lastCamPos.x, endCamPos.x, percent), 
                std::lerp<double>(self->lastCamPos.y, endCamPos.y, percent)
            );

            extrapolateGround(m_groundLayer, percent);
            extrapolateGround(m_groundLayer2, percent);
            extrapolatePlayer(m_player1, percent);

            if (m_player2)
                extrapolatePlayer(m_player2, percent);
        }
    }

    void extrapolatePlayer(PlayerObject* player, float percent) {
        if (!player) return;
        
        float endXPos = player->m_position.x + (player->m_position.x - player->m_lastPosition.x);
        float endYPos = player->m_position.y + (player->m_position.y - player->m_lastPosition.y);

        float rotateSpeed = (player->m_isBall && player->m_isBallRotating) ? 1.0f : player->m_rotateSpeed;
        float endRot = (player->m_rotationSpeed * rotateSpeed) / 240.0f;

        player->CCNode::setPosition(ccp(
            std::lerp<double>(player->m_position.x, endXPos, percent), 
            std::lerp<double>(player->m_position.y, endYPos, percent)
        ));
        
        if (player->m_mainLayer) {
            player->m_mainLayer->setRotation(std::lerp(0.0f, endRot, percent));
        }
    }

    void extrapolateGround(GJGroundLayer* ground, float percent) {
        if (!ground) return;
        
        auto self = m_fields;
        float moveBy = (self->lastCamPos.x - self->lastCamPos2.x);

        auto children = ground->getChildren();
        if (children) {
            for (auto child : CCArrayExt<CCNode*>(children)) {
                if (typeinfo_cast<CCSpriteBatchNode*>(child)) {
                    child->setPositionX(std::lerp<double>(0.0, moveBy, percent));
                }
            }
        }
    }
};
