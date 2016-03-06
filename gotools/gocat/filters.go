package main

import (
	"fmt"
	"io/ioutil"
	"os"
	"regexp"

	"github.com/davecgh/go-spew/spew"

	"gopkg.in/yaml.v2"
)

type EscapeCode int

const (
	Reset EscapeCode = iota
	Black
	Red
	Green
	Yellow
	Blue
	Magenta
	Cyan
	White
)

var filterLines []*regexp.Regexp
var highlights = make(map[*regexp.Regexp]EscapeCode)

func mustPrint(str string) bool {
	defer func() {
		if r := recover(); r != nil {
			fmt.Println("Error: Could not match filter regexp: ", r, spew.Sdump(filterLines))
			os.Exit(1)
		}
	}()

	for _, r := range filterLines {

		if r.MatchString(str) {
			return true
		}
	}
	return false
}

const keyEscape = 27

var vt100EscapeCodes = map[EscapeCode][]byte{
	Black:   []byte{keyEscape, '[', '3', '0', 'm'},
	Red:     []byte{keyEscape, '[', '3', '1', 'm'},
	Green:   []byte{keyEscape, '[', '3', '2', 'm'},
	Yellow:  []byte{keyEscape, '[', '3', '3', 'm'},
	Blue:    []byte{keyEscape, '[', '3', '4', 'm'},
	Magenta: []byte{keyEscape, '[', '3', '5', 'm'},
	Cyan:    []byte{keyEscape, '[', '3', '6', 'm'},
	White:   []byte{keyEscape, '[', '3', '7', 'm'},

	Reset: []byte{keyEscape, '[', '0', 'm'},
}

func highlightString(str string) string {
	matches := make([]EscapeCode, len(str))
	for h, att := range highlights {
		for _, match := range h.FindAllStringIndex(str, -1) {
			for i := match[0]; i < match[1]; i++ {
				matches[i] = att
			}
		}
	}

	var last EscapeCode = 0
	result := ""
	for i := 0; i < len(str); i++ {
		if matches[i] != last {
			result += string(vt100EscapeCodes[matches[i]])
			last = matches[i]
		}
		result += string(str[i])
	}
	result += string(vt100EscapeCodes[Reset])
	return result
}

type config struct {
	Filters    []string
	Highlights map[string]string
}

func readConfig(file string) {

	data, err := ioutil.ReadFile(file)
	crashIf(err)

	var conf config
	conf.Filters = []string{}
	conf.Highlights = make(map[string]string)

	crashIf(yaml.Unmarshal(data, &conf))

	for _, f := range conf.Filters {
		r, err := regexp.Compile(f)
		crashIf(err)
		filterLines = append(filterLines, r)
	}

	colorMap := map[uint8]EscapeCode{
		'd': Reset,
		'k': Black,
		'r': Red,
		'g': Green,
		'y': Yellow,
		'b': Blue,
		'm': Magenta,
		'c': Cyan,
		'w': White,
	}
	for f, color := range conf.Highlights {
		if len(color) == 0 {
			crashIf(fmt.Errorf("Highlight color can't be left blank"))
		}

		r, err := regexp.Compile(f)
		crashIf(err)

		var att EscapeCode

		att, ok := colorMap[color[0]]
		if !ok {
			crashIf(fmt.Errorf(
				"Unknown color %s. Possible colors: (d)efault, blac(k), (r)ed, (g)reen, (y)ellow, (b)lue, (m)agenta, (c)yan, (w)hite, (B)old, (U)nderline, (R)everse",
				color))
		}

		highlights[r] = att
	}
}
