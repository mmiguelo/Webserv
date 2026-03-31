#!/usr/bin/env python3

import random

ROASTS = [
    "You're the human equivalent of a participation trophy.",
    "I'd roast you, but my mom said I'm not allowed to burn trash.",
    "You have your entire life to be an idiot. Why not take today off?",
    "I'd explain it to you, but I left my crayons at home.",
    "You're not stupid — you just have bad luck thinking.",
    "I've seen better heads on a pimple.",
    "You're like a cloud — when you disappear, it's a beautiful day.",
    "If brains were taxed, you'd get a refund.",
    "You're proof that even evolution makes typos.",
    "Somewhere out there, a tree is tirelessly producing oxygen for you. You owe it an apology.",
]

print("Content-Type: text/plain; charset=utf-8")
print()
print(random.choice(ROASTS), flush=True)