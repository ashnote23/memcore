#include "scheduler.h"

using namespace std;

void Scheduler::create_user(UserId uid) {
    if (users.find(uid) != users.end()) {
        return;
    }
    users.emplace(uid, User(uid));
}

bool Scheduler::user_exists(UserId uid) const {
    return users.find(uid) != users.end();
}

const std::unordered_map<UserId, User>& Scheduler::get_users() const {
    return users;
}

bool Scheduler::card_exists(UserId uid, CardId cid) const {
    auto userIt = users.find(uid);
    if (userIt == users.end())
        return false;

    const User& user = userIt->second;

    return user.cards.find(cid) != user.cards.end();
}

bool Scheduler::topic_exists(UserId uid, TopicId tid) const {
    auto userIt = users.find(uid);
    if (userIt == users.end())
        return false;

    const User& user = userIt->second;

    return user.topics.find(tid) != user.topics.end();
}

void Scheduler::create_topic(UserId uid, const Topic& topic) {
    if (users.find(uid) == users.end()) {
        users.emplace(uid, User(uid));
    }

    users[uid].topics[topic.id] = topic;
}

void Scheduler::add_card(UserId uid, const Card& card) {
    User& user = users[uid];

    user.cards[card.id] = card;

    user.dueQueue.push({card.nextReviewDate, card.id});
}

void Scheduler::review_complete(UserId uid, CardId cardId, int rating) {
    if(users.find(uid) == users.end())
        return;

    User& user = users[uid];

    if(user.cards.find(cardId) == user.cards.end())//check if the card exists
        return;

    Card& card = user.cards[cardId];

    //Remove card from heap
    vector<CardId> temp;
    while(!user.dueQueue.empty()) {
        auto [date, id] = user.dueQueue.top();

        user.dueQueue.pop();
        if(id!=cardId)
            temp.push_back(id);
    }

    for(CardId id: temp)
        user.dueQueue.push({card.nextReviewDate, id});

    //update the repetitions
    if(rating < 2)
    {
        card.repetitions = 0;
        card.interval = 1;
    }
    else
        card.repetitions++;
    
    //update the ease factor
    card.easeFactor = max(1.3, card.easeFactor + (0.1 - (3 - rating)*(0.08 + (3 - rating) * 0.02)));


    //update interval
    if(card.repetitions == 1)
        card.interval = 1;
    else if(card.repetitions == 2)
        card.interval = 6;
    else 
        card.interval = static_cast<int>(card.interval * card.easeFactor);

    //update nextReviewDate
    card.nextReviewDate += card.interval;

    std::cout<<"Next review Date: "<<card.nextReviewDate<<"\n"<<"Ease Factor: "<<card.easeFactor<<"\n"<<"Interval: "<<card.interval<<"\n";
    //push back the card
    user.dueQueue.push({card.nextReviewDate, cardId});    
}

vector<CardId> Scheduler::get_due_cards(UserId uid, Date date, TopicId topicId) {
    vector<CardId> result;

    // //check if the user exists
    // if(users.find(uid) == users.end())
    //     return result;
    
    User& user = users[uid];

    // vector<CardId> temp;

    while(!user.dueQueue.empty()) {
        
        auto [dueDate, cardId] = user.dueQueue.top();

        if(dueDate > date)//if the card is not due yet
            break;

        user.dueQueue.pop();

        // if(topicId == -1 || card.topicId == topicId)//if the topic is -1 or the card topic matches with the topic
            result.push_back(cardId);

        // temp.push_back(cardid);//temporarly store in temp
    }

    // for(CardId id: temp)
    // {
    //     user.dueQueue.push(id);
    // }

    return result;

}
