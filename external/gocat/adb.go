package main

import (
	"io"
	"os/exec"
	"strings"
	"time"
)

func adbOneShot(command string) (string, error) {
	result, err := exec.Command("adb", strings.Split(command, " ")...).Output()
	return string(result), err
}

func adbNonstop(command string, out chan<- string) error {
	c := exec.Command("adb", strings.Split(command, " ")...)
	o, err := c.StdoutPipe()
	if err != nil {
		return err
	}

	if err := c.Start(); err != nil {
		return err
	}

	go func() {
		b := make([]byte, 1, 1)
		var currentLine string
		for {
			n, err := o.Read(b)
			if err == io.EOF {
				return
			}
			crashIf(err)

			if n == 0 {
				time.Sleep(10 * time.Millisecond)
				continue
			}
			if b[0] == '\n' {
				out <- currentLine
				currentLine = ""
			} else {
				currentLine += string(b[0])
			}
		}
	}()

	go func() {
		crashIf(c.Wait())
	}()
	return nil
}
