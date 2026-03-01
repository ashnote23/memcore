#include <grpcpp/grpcpp.h>
#include "proto/flashcards.grpc.pb.h"
#include "review_service.h"

class FlashcardServiceImpl final 
    : public flashcards::FlashcardService::Service {
private:
    ReviewService& reviewService;
public:
    FlashcardServiceImpl(ReviewService& rs) : reviewService(rs) {}

    grpc::Status ReviewComplete(
        grpc::ServerContext* context,
        const flashcards::ReviewCompleteRequest* request,
        flashcards::ReviewCompleteResponse* response) override {
        // call reviewService and fill response
        int user_id = request->user_id();
        int card_id = request->card_id();
        int rating = request->rating();
        reviewService.review_card(user_id, card_id, rating);

        Card card = reviewService.get_card(user_id, card_id);
        response->set_next_review_date(card.nextReviewDate);
        return grpc::Status::OK;
    }

    grpc::Status AddCard(
        grpc::ServerContext* context,
        const flashcards::AddCardRequest* request,
        flashcards::AddCardResponse* response) override {
        // call reviewService and fill response
        int user_id = request->user_id();
        int card_id = request->card_id();
        int topic_id = request->topic_id();
        reviewService.add_card(user_id, card_id, topic_id);

        response->set_success(true);
        return grpc::Status::OK;
    }

    grpc::Status GetDueCards(
        grpc::ServerContext* context,
        const flashcards::GetDueCardsRequest* request,
        flashcards::GetDueCardsResponse* response) override {
        // call reviewService and fill response
        int user_id = request->user_id();
        int date = request->date();
        int topic_id = request->topic_id();
        std::vector<CardId> dueCards = reviewService.get_due_cards(user_id, date, topic_id);

        for (CardId id : dueCards) {
            response->add_card_ids(id);
        }

        return grpc::Status::OK;
    }

    grpc::Status CreateTopic(
        grpc::ServerContext* context,
        const flashcards::CreateTopicRequest* request,
        flashcards::CreateTopicResponse* response) override {
        // call reviewService and fill response
        int user_id = request->user_id();
        int topic_id = request->topic_id();
        std::string name = request->name();
        reviewService.create_topic(user_id, topic_id, name);

        response->set_success(true);
        return grpc::Status::OK;
    }
};