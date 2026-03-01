package main

import (
	"encoding/json"
    "net/http"
	"log"
	"context"
    "google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"
	pb "memcore/api/proto"
)

type Server struct {
	client pb.FlashcardServiceClient
}

type ReviewRequest struct {
    UserId int `json:"user_id"`
    CardId int `json:"card_id"`
    Rating int `json:"rating"`
}

type ReviewResponse struct {
	NextReviewDate int `json:"next_review_date"`
}

type AddCardRequest struct {
	UserId int `json:"user_id"`
    CardId int `json:"card_id"`
    TopicId int `json:"topic_id"`
}

type AddCardResponse struct {
	Success bool `json:"success"`
}

type GetDueCardsRequest struct {
	UserId int `json:"user_id"`
    Date int `json:"date"`
    TopicId int `json:"topic_id"`
}

type GetDueCardsResponse struct {
	CardIds []int `json:"card_ids"`
}

type CreateTopicRequest struct {
	UserId int `json:"user_id"`
    TopicId int `json:"topic_id"`
	Name string `json:"name"`
}

type CreateTopicResponse struct {
	Success bool `json:"success"`
}


// 1. A handler function — processes one type of request
func (s *Server) handleReview(w http.ResponseWriter, r *http.Request) {
    if r.Method == "POST" {
		// read request, do work, write response
		var req ReviewRequest
		json.NewDecoder(r.Body).Decode(&req)// now req.UserId, req.CardId, req.Rating are populated

		grpcReq := &pb.ReviewCompleteRequest{
            UserId: int32(req.UserId),
            CardId: int32(req.CardId),
            Rating: int32(req.Rating),
        }

		grpcRes, err := s.client.ReviewComplete(context.Background(), grpcReq)
        if err != nil {
            http.Error(w, err.Error(), http.StatusInternalServerError)
            return
        }

		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(ReviewResponse{
			NextReviewDate: int(grpcRes.NextReviewDate),
		})
	} else {
		return
	}
}

func (s *Server) handleAddCard(w http.ResponseWriter, r *http.Request) {
	if r.Method == "POST" {
		var add AddCardRequest
		json.NewDecoder(r.Body).Decode(&add)

		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(AddCardResponse{Success: true})
	} else {
		return
	}
}

func (s *Server) handleGetDueCards(w http.ResponseWriter, r *http.Request) {
	if r.Method == "GET" {
		var dueCard GetDueCardsRequest
		json.NewDecoder(r.Body).Decode(&dueCard)
		
		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(GetDueCardsResponse{CardIds: []int{1, 2}})
	} else {
		return
	}
}

func (s *Server) handleCreateTopic(w http.ResponseWriter, r *http.Request) {
	if r.Method == "POST" {
		var topic CreateTopicRequest
		json.NewDecoder(r.Body).Decode(&topic)

		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(CreateTopicResponse{Success: true})
	} else {
		return
	}
}

func main() {

	conn, err := grpc.Dial("localhost:50051", grpc.WithTransportCredentials(insecure.NewCredentials()))
    if err != nil {
        log.Fatalf("Failed to connect: %v", err)
    }
    defer conn.Close()

	client := pb.NewFlashcardServiceClient(conn)
	server := &Server{client: client}
	http.HandleFunc("/review", server.handleReview)
	http.HandleFunc("/card", server.handleAddCard)
	http.HandleFunc("/due-cards", server.handleGetDueCards)
	http.HandleFunc("/topic", server.handleCreateTopic)
	http.ListenAndServe(":8080", nil)
}

