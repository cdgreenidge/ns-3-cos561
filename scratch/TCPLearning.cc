#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <math.h>

static int n_prototypes = 10;
static int n_actions = 5;
static int n_state_intervals = 10;
static int state_dim = 4;
static int cwnd_action_mappings[5] = {-1, 0, 5, 10, 20};
static int prototype_actions[10];
static int prototype_states[10][4];
static double thetas[10];
static double discount_factor = 0.99;
static double lr = 0.5;
static double epsilon_thresh = 0.6;
//static int prev_interval_utility = 0.0;
//static int curr_interval_utility = 0.0;

static int cwnd = 10;
static double variance = 1;

struct state_action_pair {
    int state[4];
    int action;
};

static state_action_pair prev;
static int curr_state[4];

float utility_diff() {
    return 1;
}

void update_curr_state() {
    for (int j = 0; j < state_dim; j++) {
        curr_state[j] = rand() % n_state_intervals;
    }
}

// get membership grade between given prototype and state action pair
double getMembershipGrade(int prototype, int state[4], int action) {
    double diff = 0.0;
    double membership;
    diff += pow((action - prototype_actions[prototype]), 2.0);

    for (int i = 0; i < state_dim; i++) {
        diff += pow((state[i] - prototype_states[prototype][i]), 2.0);
    }
    membership = exp(-diff/(2*variance));
    return membership;
}


void fuzzyKanerva(float delta_utility) {
    // get current state and reward
    update_curr_state();
    double reward = 0.0;
    if (delta_utility > 0) {
        reward = 2.0;
    } else {
        reward = -2.0;
    }
    
    // for each prototype, calculate membership and q value for pair (prev_s, prev_a)
    double prev_memberships[n_prototypes];
    double prev_Q = 0.0;

    for (int i = 0; i < n_prototypes; i++) {
        prev_memberships[i] = getMembershipGrade(i, prev.state, prev.action);
        prev_Q += prev_memberships[i] * thetas[i];
    }
    

    int argmax_curr_Q;
    double max_curr_Q;
    
    // for each pair (s, a'), calculate membership and q value
    for (int a = 0; a < n_actions; a++) {
        double curr_Q_a = 0.0;
        for (int i = 0; i < n_prototypes; i++) {
            double membership = getMembershipGrade(i, curr_state, a);
            curr_Q_a += membership * thetas[i];
        }
        if (a == 0) {
            max_curr_Q = curr_Q_a;
            argmax_curr_Q = a;
        } else {
            if (max_curr_Q < curr_Q_a) {
                max_curr_Q = curr_Q_a;
                argmax_curr_Q = a;
            }
        }
    }
    
    // compute delta value
    double delta = reward + (discount_factor * max_curr_Q) - prev_Q;
    
    // update thetas for prototypes
    for (int i = 0; i < n_prototypes; i++) {
        double theta_update = prev_memberships[i] * lr * delta;
        thetas[i] = theta_update;
    }
    
    // epsilon greedy policy for action selection
    if (((float) rand()/RAND_MAX) > epsilon_thresh) {
        prev.action = argmax_curr_Q;
    } else {
        prev.action = rand() % n_actions;
    }
    for (int i = 0; i < state_dim; i++) {
        prev.state[i] = curr_state[i];
    }
    
}

void change_cwnd() {
    cwnd += cwnd_action_mappings[prev.action];
}

int
main (int argc, char *argv[])
{
    srand (0);
    
    //choose random values for prototypes
    for (int i = 0; i < n_prototypes; i++) {
        prototype_actions[i] = rand() % n_actions;
        for (int j = 0; j < state_dim; j++) {
            prototype_states[i][j] = rand() % n_state_intervals;
        }
    }

    // set previous state and action
    for (int j=0; j < state_dim; j++) {
        prev.state[j] = 0;
    }
    prev.action = 1;
    
    // for ack recieved in congestion avoidance in TCP NewReno
    // (this will need to be hooked up to our congestion algorithm)
    int n_acks = 5;
    for (int ack = 0; ack < n_acks; ack++) {
        // check if utility has changed
        float delta_utility = utility_diff();
        if (delta_utility != 0) {
            fuzzyKanerva(delta_utility);
        }
        // update congestion window
        change_cwnd();
    }
    
    return 0;
}
