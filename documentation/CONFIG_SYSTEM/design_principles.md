# Design Principles of our Configuration System

This document outlines the philosophy behind how the configuration system works.
The goal is to keep things simple, predictable and easy to debug.

### Separation of Responsibilities 

The config system is the behavioral layer within the server architecture.
It defines how the server responds to requests, but it doesn't handle networking or protocol parsing.
The server divides work into different responsibilities:

- The **network layer** deals with connections.
- The **HTTP layer** parses requests into structured data (parsing the request into method, URI, headers).
- The **configuration layer** receives the parsed data and defines how the server behaves to such requests.

That separation should make each part manageable and easier to debug.

### Deterministic Behavior

One of the main goals here is predictability. Asking the server the same question twice, should output the exact same answer twice.

Given the same port, method, and URI, the configuration system should always produce the same result. With no surprises and no hidden behavior.
The system selects one server block and one matching location block, and produces a clear execution decision.

Every decision the server makes should be easy for us to trace back to a specific rule in the config file.

### Simplicity

We are told in the subject to study NGINX behavior, not to copy it topdown. It contains complex tool that are not required for this project.
Complex features like Regex matching, nested locations (putting folders inside folders in the config), and rewriting URLs on the fly are left out.
Our goal is a stable, working program. By keeping the rules simple, it's easier for us to build it and verify that it works correctly.

### Hierarchy

The configuration model follows a simple hierarchy:

- A `server` block sets the default rules for a whole website.
- The `location` blocks handle specifics, and override default rules.

If a location block doesn't define a rule, it uses the server's rule. But if the location block does define a rule, that specific rule overrides the default set by the server block. This stops us from having to repeat ourselves constantly in the config file.

---

### Strict Grammar and Validation

Our config file parser will be strict. If there's a missing semicolon or a closing bracket }, the server will refuse to start and will tell you where the error is.
It is better for the server to crash immediately when we turn it on than to run half-broken and behave unpredictably.
We want it to fail fast so we can fix it fast.

---

### Internal Data Model

When a request comes in, we shouldn't read the text config file at every execution.

When the server starts, we read the text file once and convert it into organized objects internally (like a "Configuration" object holding "Server" objects, etc.).
Once that's done, the rest of our code only looks at those objects, never the raw text file. This keeps our code organized and separates "reading the file" from "making decisions based on the file."

---

### Location Matching Strategy

Location matching is simple by design.
We use a strategy called **longest prefix match** - the same method used by NGINX - we match the most specific path.

If we have rules for `/images` and `/images/icons`:
A request for `/images/cat.jpg` uses the first rule.
A request for `/images/icons/logo.png` uses the second rule, because it's a longer, more specific match.
This is predictable and relatively easy for us to implement.

---

### Conceptual Role

The configuration system works as a small policy engine. It translates a parsed HTTP request into an execution plan based on declarative rules defined by the user.
It doesn’t execute the decision and it won’t generate the response.

It doesn't actually fetch  files or sends responses. It just makes the plan and hands it off to the next part of the system to execute.