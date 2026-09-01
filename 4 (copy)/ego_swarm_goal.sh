#!/bin/bash
while(true)
do
    python3 ego_swarm_goal.py iris 0 -1 5 1.0&
    python3 ego_swarm_goal.py iris 1 4.4 -5 1.0&
    python3 ego_swarm_goal.py iris 2 1 -5 1.0&
    python3 ego_swarm_goal.py iris 3 0 6 1.0
done
