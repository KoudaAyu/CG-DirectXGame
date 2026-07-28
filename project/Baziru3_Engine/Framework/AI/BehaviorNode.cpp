#include "BehaviorNode.h"
#include "Blackboard.h"

namespace BaziruEngine::AI {

BehaviorStatus BehaviorNode::Tick(std::shared_ptr<Blackboard> blackboard) {
    if (status_ == BehaviorStatus::Invalid) {
        OnInitialize(blackboard);
        status_ = BehaviorStatus::Running;
    }

    status_ = Update(blackboard);

    if (status_ != BehaviorStatus::Running) {
        OnTerminate(blackboard, status_);
    }

    return status_;
}

} // namespace BaziruEngine::AI
