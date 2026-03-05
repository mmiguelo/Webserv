#!/bin/zsh

echo "\n1. Basic GET"
curl -v http://localhost:8080/

echo "\n2. GET with Query"
curl -v "http://localhost:8080/search?q=test"

echo "\n3. POST with Body"
curl -v -X POST -d "Hello World" http://localhost:8080/submit

echo "\n4. PATCH (unsupported method)"
curl -v -X PATCH http://localhost:8080/

echo "\n5. Large Header (Header Too Large)"
curl -v -H "X-Big: $(python3 -c 'print("A"*9000)')" http://localhost:8080/

echo "\n6. No Body (GET /nobody)"
curl -v http://localhost:8080/nobody

echo "\n7. Custom Header"
curl -v -H "X-Test-Header: test-value" http://localhost:8080/custom

echo "\n8. Malformed Request"
printf "GARBAGE DATA\r\n\r\n" | nc localhost 8080

echo "\n9. Invalid HTTP Version"
printf "GET / HTTP/2.0\r\nHost: localhost\r\n\r\n" | nc localhost 8080

echo "\n10. Missing Host Header (HTTP/1.1)"
printf "GET / HTTP/1.1\r\n\r\n" | nc localhost 8080