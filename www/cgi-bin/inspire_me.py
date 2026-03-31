#!/usr/bin/env python3

import random

INSPIRE = [
    "Your potential is endless — keep reaching for the stars.",
    "Every step forward, no matter how small, is progress.",
    "Believe in yourself. You've survived 100% of your worst days so far.",
    "Your ideas can change the world, one spark at a time.",
    "You are stronger than you think and braver than you feel.",
    "Even mountains are climbed one step at a time.",
    "Dream big. The world needs your vision.",
    "Mistakes are just proof that you're trying.",
    "Your kindness today can inspire a thousand tomorrows.",
    "Keep going — amazing things are just around the corner.",
]

print("Content-Type: text/plain; charset=utf-8")
print()
print(random.choice(INSPIRE), flush=True)