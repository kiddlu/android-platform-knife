package main

import (
	"fmt"
	"log"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"sync"
	"time"
)

func crashIf(err error) {
	if err == nil {
		return
	}

	fmt.Printf("Error: %s\n", err.Error())
	os.Exit(1)
}

/*
type Log struct {
	sync.Mutex
	lines []string
}
var logs map[string]Log
var logsMutex sync.Mutex
*/

var lines []string
var linesMutex sync.Mutex

func main() {
	cur_dir, _ := filepath.Abs(filepath.Dir(os.Args[0]))
	//fmt.Println(cur_dir + "/config.yaml")
	cfg_path := cur_dir + "/config.yaml"
	readConfig(cfg_path)

	//fmt.Println(adbOneShot("shell ps"))

	//return

	// start reading adb output
	adbChan := make(chan string, 10000)
	crashIf(adbNonstop("logcat", adbChan))

	handler := func(w http.ResponseWriter, r *http.Request) {
		lineNum := 0
		var l string

		for {
			linesMutex.Lock()
			mustSend := lineNum < len(lines)
			if mustSend {
				l = lines[lineNum]
				lineNum++
			}
			linesMutex.Unlock()

			if !mustSend {
				time.Sleep(50 * time.Millisecond)
				continue
			}

			fmt.Fprintln(w, l)
			if flusher, ok := w.(http.Flusher); ok {
				flusher.Flush()
			}
		}
	}

	server := http.Server{
		Addr:         ":10001",
		WriteTimeout: 4 * time.Hour,
		Handler:      http.HandlerFunc(handler),
	}

	go func() {
		log.Fatal(server.ListenAndServe())
	}()

	if runtime.GOOS == "darwin" {
		go exec.Command("open", "http://localhost:10001").Run()
	}

	for {
		line := <-adbChan
		linesMutex.Lock()
		lines = append(lines, line)
		linesMutex.Unlock()

		if mustPrint(line) {
			fmt.Println(highlightString(line))
		}
	}

}
