## 1. Multiple POST fields

``` bash
curl -X POST http://localhost:8080 -d "a=1" -d "b=2"
```

Check: - Body is concatenated correctly - No crash or ignored data

Expect: - Server processes both fields correctly

------------------------------------------------------------------------

## 2. Missing Content-Length

``` bash
printf "POST / HTTP/1.1\r\nHost: localhost\r\n\r\nBODY" | nc localhost 8080
```

Check: - Server does not hang

Expect: - 400 Bad Request

------------------------------------------------------------------------

## 3. Wrong Content-Length

``` bash
printf "POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 100\r\n\r\nabc" | nc localhost 8080
```

Check: - No crash - Proper handling of incomplete body

Expect: - Timeout or error response

------------------------------------------------------------------------

## 4. Chunked encoding

``` bash
printf "POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nHello\r\n0\r\n\r\n" | nc localhost 8080
```

Check: - No crash

Expect: - Either correct parsing or 400/501 response

------------------------------------------------------------------------

## 5. Large headers

``` bash
python3 -c "print('GET / HTTP/1.1\r\nHost: ' + 'A'*10000 + '\r\n\r\n')" | nc localhost 8080
```

Check: - No buffer overflow - No crash

Expect: - Proper error or handled request

------------------------------------------------------------------------

## 6. Multiple connections

``` bash
for i in {1..100}; do curl http://localhost:8080 & done
wait
```

Check: - Server stability - No dropped connections

Expect: - All requests handled

------------------------------------------------------------------------

## 7. Slow request

``` bash
(printf "GET / HTTP/1.1\r\nHost: localhost\r\n"; sleep 10; printf "\r\n") | nc localhost 8080
```

Check: - Server does not block entirely

Expect: - Timeout or independent handling

------------------------------------------------------------------------

## 8. Pipeline requests

``` bash
printf "GET / HTTP/1.1\r\nHost: localhost\r\n\r\nGET / HTTP/1.1\r\nHost: localhost\r\n\r\n" | nc localhost 8080
```

Check: - Multiple requests handled correctly

Expect: - Both responses or clean connection close

------------------------------------------------------------------------

## 9. Invalid method

``` bash
curl -X FAKE http://localhost:8080
```

Check: - Method validation

Expect: - 405 Method Not Allowed

------------------------------------------------------------------------

## 10. Directory traversal

``` bash
curl http://localhost:8080/../../../../etc/passwd
```

Check: - Security

Expect: - 403 Forbidden or 404 Not Found

------------------------------------------------------------------------

## 11. DELETE non-existent file

``` bash
curl -X DELETE http://localhost:8080/file_that_does_not_exist
```

Check: - Error handling

Expect: - 404 Not Found

------------------------------------------------------------------------

## 12. File upload

``` bash
curl -X POST http://localhost:8080/upload -F "file=@/etc/hosts"
```

Check: - Multipart parsing - File integrity

Expect: - File correctly received

------------------------------------------------------------------------

## 13. Missing Host header

``` bash
printf "GET / HTTP/1.1\r\n\r\n" | nc localhost 8080
```

Check: - HTTP compliance

Expect: - 400 Bad Request

------------------------------------------------------------------------

## 14. Keep-Alive

``` bash
curl -v http://localhost:8080 --header "Connection: keep-alive"
```

Check: - Connection reuse

Expect: - Proper keep-alive handling

------------------------------------------------------------------------

## 15. Large body

``` bash
dd if=/dev/zero bs=1M count=10 | curl -X POST http://localhost:8080 --data-binary @-
```

Check: - Memory handling - Max body size enforcement

Expect: - Accept or reject based on config

------------------------------------------------------------------------

## 16. CGI test

``` bash
curl http://localhost:8080/cgi-bin/script.py
```

Check: - Execution - Environment variables

Expect: - Correct output

------------------------------------------------------------------------

## 17. HEAD request

``` bash
curl -I http://localhost:8080
```

Check: - Headers only

Expect: - No response body

------------------------------------------------------------------------

## 18. Combined stress test

``` bash
for i in {1..50}; do
  (printf "POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\nabc"; sleep 2) | nc localhost 8080 &
done
wait
```

Check: - Concurrency - Stability

Expect: - No crash or deadlock
