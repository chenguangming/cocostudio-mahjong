//
// Created by farmer on 2018/7/5.
//

#include "AIEngine.h"
#include "IPlayer.h"
#include "DelayCall.h"

AIEngine::AIEngine() {
    m_GameEngine = GameEngine::GetGameEngine();
    m_MeChairID = INVALID_CHAIR;
    initGame();
}

AIEngine::~AIEngine() {
    cocos2d::log("~AIEngine");
}

/**
 * 初始化游戏变量
 */
void AIEngine::initGame(){
    m_cbLeftCardCount = 0;
    m_cbBankerChair = INVALID_CHAIR;
    memset(&m_cbWeaveItemCount, 0, sizeof(m_cbWeaveItemCount));
    memset(&m_cbDiscardCount, 0, sizeof(m_cbDiscardCount));
    for (uint8_t i = 0; i < GAME_PLAYER; i++) {
        memset(m_cbCardIndex[i], 0, sizeof(m_cbCardIndex[i]));
        memset(m_WeaveItemArray[i], 0, sizeof(m_WeaveItemArray[i]));
        memset(m_cbDiscardCard[i], 0, sizeof(m_cbDiscardCard[i]));
    }

    m_darkCards.clear();
    for (uint8_t i=0; i<MAX_INDEX; i++) {
        m_darkCards.push_back(4);
    }
}

void AIEngine::setIPlayer(IPlayer *pIPlayer) {
    m_MePlayer = pIPlayer;
}

bool AIEngine::onUserEnterEvent(IPlayer *pIPlayer) {
    m_MeChairID = m_MePlayer->getChairID();
    return true;
}

bool AIEngine::onGameStartEvent(CMD_S_GameStart GameStart) {
    cocos2d::log("机器人接收到游戏开始事件");
    initGame();
    m_cbLeftCardCount = GameStart.cbLeftCardCount;
    m_cbBankerChair = GameStart.cbBankerUser;
    GameLogic::switchToCardIndex(GameStart.cbCardData, MAX_COUNT - 1, m_cbCardIndex[m_MeChairID]);
    for (int i=0; i<MAX_INDEX; i++) {
        uint8_t num = m_cbCardIndex[m_MeChairID][i];
        if (num > 0) {
            // 自己的牌，全部标记已见
            markCardShow(i, num);
        }
    }

    return true;
}

void AIEngine::markCardShow(uint8_t idx, u_int8_t cnt) {
    m_darkCards[idx] -= cnt;
    cocos2d::log("机器人%d 标记 %d 张 %s 已见, 剩余%d张", m_MeChairID, cnt, GameLogic::getCardNameByIndex(idx), m_darkCards[idx]);
}

void AIEngine::dumpMyCards() const {
    char cardNameArray[MAX_INDEX*10];
    char *p = cardNameArray;
    for (int i=0; i<MAX_INDEX; i++) {
        int num = m_cbCardIndex[m_MeChairID][i];
        for (int j=0; j<num; j++) {
            p += sprintf(p, "%s,", GameLogic::getCardNameByIndex(i));
        }
    }
    cocos2d::log("机器人%d 的牌: %s", m_MeChairID, cardNameArray);
}

void AIEngine::dumpDiscardCards() const {
    char cardNameArray[MAX_DISCARD*6];
    char *p = cardNameArray;
    int count = m_cbDiscardCount[m_MeChairID];

    for (int i=0; i<count; i++) {
        p += sprintf(p, "%s,", GameLogic::getCardName(m_cbDiscardCard[m_MeChairID][i]));
    }
    cocos2d::log("机器人%d 出过的牌: %s", m_MeChairID, cardNameArray);
}


bool AIEngine::onSendCardEvent(CMD_S_SendCard SendCard) {
    if (SendCard.cbCurrentUser == m_MeChairID) { //出牌
        m_cbLeftCardCount--;
        dumpMyCards();
        if (SendCard.cbCurrentUser == m_MeChairID) {
            cocos2d::log("机器人%d 摸牌: %s", m_MeChairID, GameLogic::getCardName(SendCard.cbCardData));
            m_cbCardIndex[m_MeChairID][GameLogic::switchToCardIndex(SendCard.cbCardData)]++;

            markCardShow(GameLogic::switchToCardIndex(SendCard.cbCardData), 1);
        }
        if (SendCard.cbActionMask != WIK_NULL) {//发的牌存在动作，模拟发送操作通知
            CMD_S_OperateNotify OperateNotify;
            memset(&OperateNotify, 0, sizeof(CMD_S_OperateNotify));
            OperateNotify.cbActionMask = SendCard.cbActionMask;
            OperateNotify.cbActionCard = SendCard.cbCardData;
            OperateNotify.cbGangCount = SendCard.cbGangCount;
            memcpy(OperateNotify.cbGangCard, SendCard.cbGangCard, sizeof(OperateNotify.cbGangCard));
            onOperateNotifyEvent(OperateNotify);
        } else {
            DelayCall::add([this]() { discardCard(); }, time(NULL) % 2 + 0.8f);
        }
    }
    return true;
}

/**
 * 出牌事件
 * @param OutCard
 * @return
 */
bool AIEngine::onOutCardEvent(CMD_S_OutCard OutCard) {
    if (OutCard.cbOutCardUser == m_MeChairID) {
        cocos2d::log("机器人 %d 出牌 %s", m_MeChairID, GameLogic::getCardName(OutCard.cbOutCardData));

        m_cbCardIndex[m_MeChairID][GameLogic::switchToCardIndex(OutCard.cbOutCardData)]--;
    } else {
        // 对手出牌，标记可见
        markCardShow(GameLogic::switchToCardIndex(OutCard.cbOutCardData), 1);
    }
    m_cbDiscardCard[OutCard.cbOutCardUser][m_cbDiscardCount[OutCard.cbOutCardUser]++] = OutCard.cbOutCardData;
    return true;
}

/**
 * 操作通知事件
 * @param OperateNotify
 * @return
 */
bool AIEngine::onOperateNotifyEvent(CMD_S_OperateNotify OperateNotify) {
    cocos2d::log("机器人%d 接收到操作通知事件", m_MeChairID);
    if (OperateNotify.cbActionMask == WIK_NULL) {
        return true; //无动作
    }
    CMD_C_OperateCard OperateCard;
    memset(&OperateCard, 0, sizeof(CMD_C_OperateCard));     //重置内存
    OperateCard.cbOperateUser = m_MeChairID;   //操作的玩家
    if ((OperateNotify.cbActionMask & WIK_H) != 0) {        //胡的优先级最高
        OperateCard.cbOperateCode = WIK_H;
        OperateCard.cbOperateCard = OperateNotify.cbActionCard;
    } else if ((OperateNotify.cbActionMask & WIK_G) != 0) { //杠的优先级第二
        OperateCard.cbOperateCode = WIK_G;
        OperateCard.cbOperateCard = OperateNotify.cbGangCard[0];//杠第一个
    } else if ((OperateNotify.cbActionMask & WIK_P) != 0) { //碰的优先级第三
        OperateCard.cbOperateCode = WIK_P;
        OperateCard.cbOperateCard = OperateNotify.cbActionCard;
    }
    return m_GameEngine->onUserOperateCard(OperateCard);
}

/**
 * 操作结果事件
 * @param OperateResult
 * @return
 */
bool AIEngine::onOperateResultEvent(CMD_S_OperateResult OperateResult) {
    cocos2d::log("机器人%d 接收到操作结果事件", m_MeChairID);
    tagWeaveItem weaveItem;
    memset(&weaveItem, 0, sizeof(tagWeaveItem));
    switch (OperateResult.cbOperateCode) {
        case WIK_NULL: {
            break;
        }
        case WIK_P: {
            weaveItem.cbWeaveKind = WIK_P;
            weaveItem.cbCenterCard = OperateResult.cbOperateCard;
            weaveItem.cbPublicCard = TRUE;
            weaveItem.cbProvideUser = OperateResult.cbProvideUser;
            weaveItem.cbValid = TRUE;
            m_WeaveItemArray[OperateResult.cbOperateUser][m_cbWeaveItemCount[OperateResult.cbOperateUser]++] = weaveItem;
            if (OperateResult.cbOperateUser == m_MeChairID) { //自己出牌操作
                uint8_t cbReomveCard[] = {OperateResult.cbOperateCard, OperateResult.cbOperateCard};
                GameLogic::removeCard(m_cbCardIndex[OperateResult.cbOperateUser], cbReomveCard, sizeof(cbReomveCard));
                DelayCall::add([this]() { discardCard(); }, time(NULL) % 2 + 0.5f);
            } else {
                // 对手碰牌，有2张新牌（碰牌的一对）标记可见; 自己碰时，三张牌肯定都是之前标记过的（自己摸牌或者对手打牌时），无需重新标记
                markCardShow(GameLogic::switchToCardIndex(OperateResult.cbOperateCard), 2);
            }
            break;
        }
        case WIK_G: {
            weaveItem.cbWeaveKind = WIK_G;
            weaveItem.cbCenterCard = OperateResult.cbOperateCard;
            uint8_t cbPublicCard = (OperateResult.cbOperateUser == OperateResult.cbProvideUser) ? FALSE : TRUE;
            int j = -1;
            for (int i = 0; i < m_cbWeaveItemCount[OperateResult.cbOperateUser]; i++) {
                tagWeaveItem tempWeaveItem = m_WeaveItemArray[OperateResult.cbOperateUser][i];
                if (tempWeaveItem.cbCenterCard == OperateResult.cbOperateCard) {   //之前已经存在
                    cbPublicCard = TRUE;
                    j = i;
                }
            }
            weaveItem.cbPublicCard = cbPublicCard;
            weaveItem.cbProvideUser = OperateResult.cbProvideUser;
            weaveItem.cbValid = TRUE;
            if (j == -1) {
                m_WeaveItemArray[OperateResult.cbOperateUser][m_cbWeaveItemCount[OperateResult.cbOperateUser]++] = weaveItem;
            } else {
                m_WeaveItemArray[OperateResult.cbOperateUser][j] = weaveItem;
            }
            if (OperateResult.cbOperateUser == m_MeChairID) {  //自己
                GameLogic::removeAllCard(m_cbCardIndex[OperateResult.cbOperateUser], OperateResult.cbOperateCard);
            } else {
                // 对手杠牌，有3张新牌标记可见; 自己杠时，所有牌肯定都是之前标记过的（自己摸牌或者对手打牌时）
                markCardShow(GameLogic::switchToCardIndex(OperateResult.cbOperateCard), 3);
            }
            break;
        }
        case WIK_H: {
            break;
        }
        default:
            break;
    }
    return true;
}

/**
 * 结束
 * @param GameEnd
 * @return
 */
bool AIEngine::onGameEndEvent(CMD_S_GameEnd& GameEnd) {
    cocos2d::log("机器人接收到游戏结束事件");
    return true;
}

// 计算两张牌的距离
int AIEngine::calcCardGap(u_int8_t a, u_int8_t b) {
    // 同种牌型
    if ((a & 0xF0) == (b & 0xF0)) {
        // 字牌，只有成对的可能性
        if ((a & 0xF0) == 0x30) {
            if (a == b) {
                return 0;
            } else {
                return 4;
            }
        }

        return abs(a - b);
    }

    // 不同牌型，直接认为是最大距离（不计分）
    return 4;
}

// 根据手牌的权值推荐出牌
int AIEngine::suggestDiscard() {
    int shoupai[MAX_COUNT]; // 手牌的数组
    int shoupai_cnt = 0;
    int min_score = INT32_MAX, bad_card = 0;

    memset(shoupai, -1, MAX_COUNT);
    // 清点手中的牌，构成连续数组
    for (int i=0; i<MAX_INDEX; i++) {
        int num = m_cbCardIndex[m_MeChairID][i];
        for (int j=0; j<num; j++) {
            shoupai[shoupai_cnt++] = GameLogic::switchToCardData(i);
        }
    }

    for (int c=0; c<shoupai_cnt; c++) {
        int score = 0;
        int card = shoupai[c];

        // 遍历自己手里的牌，计算每张牌的价值（彼此越靠近的牌，相互价值就大）
        for (int cc=0; cc<shoupai_cnt; cc++) {
            int gap = calcCardGap(shoupai[cc], card);
            if (gap < 3) {
                score += WEIGHT[gap] * SHOU_PAI_SCALE;
            }
        }

        // 遍历所有未见过的牌，计算这些牌与手里牌匹配的可能性
        for (auto it = m_darkCards.cbegin(); it != m_darkCards.end(); ++it) {
            if (*it <= 0) {
                continue;
            }

            int dark_card = GameLogic::switchToCardData(it - m_darkCards.cbegin());
            for (int j=0; j<*it; j++) {
                int gap = calcCardGap(dark_card, card);
                if (gap < 3) {
                    score += WEIGHT[gap];
                }
            }
        }

        if (score < min_score) {
            min_score = score;
            bad_card = c;
        }

        cocos2d::log("机器人%d 牌 %s 权值: %d", m_MeChairID, GameLogic::getCardName(shoupai[c]), score);
    }
    cocos2d::log("机器人%d 建议打出 %s(%d), 权值: %d", m_MeChairID, GameLogic::getCardName(shoupai[bad_card]), shoupai[bad_card], min_score);

    return shoupai[bad_card];
}


/**
 * 出牌
 * @param f
 */
void AIEngine::discardCard() {
    CMD_C_OutCard OutCard;
    memset(&OutCard, 0, sizeof(CMD_C_OutCard));
    OutCard.cbCardData = suggestDiscard();

    m_GameEngine->onUserOutCard(OutCard);
    dumpDiscardCards();
}

