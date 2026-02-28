package main

import (
    "encoding/json"
    "net/http"
)

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
func handleReview(w http.ResponseWriter, r *http.Request) {
    if r.Method == "POST" {
		// read request, do work, write response
		var req ReviewRequest
		json.NewDecoder(r.Body).Decode(&req)// now req.UserId, req.CardId, req.Rating are populated

		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(ReviewResponse{NextReviewDate: 42})
	} else {
		return
	}
}

func handleAddCard(w http.ResponseWriter, r *http.Request) {
	if r.Method == "POST" {
		var add AddCardRequest
		json.NewDecoder(r.Body).Decode(&add)

		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(AddCardResponse{Success: true})
	} else {
		return
	}
}

func handleGetDueCards(w http.ResponseWriter, r *http.Request) {
	if r.Method == "GET" {
		var dueCard GetDueCardsRequest
		json.NewDecoder(r.Body).Decode(&dueCard)
		
		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(GetDueCardsResponse{CardIds: []int{1, 2}})
	} else {
		return
	}
}

func handleCreateTopic(w http.ResponseWriter, r *http.Request) {
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
	http.HandleFunc("/review", handleReview)
	http.HandleFunc("/card", handleAddCard)
	http.HandleFunc("/due-cards", handleGetDueCards)
	http.HandleFunc("/topic", handleCreateTopic)
	http.ListenAndServe(":8080", nil)
}

