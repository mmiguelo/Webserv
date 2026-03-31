#!/usr/bin/env python3

import random

SURPRISE = [
    "Guess what? You just became the main character of your own adventure!",
    "Did you know? Today is statistically your luckiest day.",
    "A penguin just waved at you in spirit. How cool is that?",
    "Plot twist: Everything you've been working on is about to pay off.",
    "Surprise! You're officially awesome in someone's eyes right now.",
    "A random dance break is highly recommended. Go ahead.",
    "Something wonderful is heading your way. Keep your eyes open!",
    "Unexpected joy just RSVP'd to your day. Don't keep it waiting.",
    "You've just unlocked a secret level of life. Enjoy!",
    "Surprise! You remembered to smile today — and that's contagious.",
]

print("Content-Type: text/plain; charset=utf-8")
print()
print(random.choice(SURPRISE), flush=True)