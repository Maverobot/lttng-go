package main

/*
   #cgo pkg-config: babeltrace2
   #cgo LDFLAGS: -L. -lbabeltrace2 -ljson-c
   #include <read_live_stream.h>
*/
import "C"

import (
	"fmt"
	"log"
	"os"
	"strings"
	"time"
	"unsafe"

	tea "github.com/charmbracelet/bubbletea"
	"github.com/muesli/reflow/wordwrap"
)

const maxWidth = 80

var (
	relay_data = C.relay_data{}
	graph      *C.bt_graph
)

func main() {
	/* Create the trace processing graph */
	url := C.CString(os.Args[1])
	defer C.free(unsafe.Pointer(url))
	graph = C.create_graph(url, &relay_data)
	defer C.bt_graph_put_ref(graph)
	if graph == nil {
		fmt.Fprintf(os.Stderr, "No graph can be created. Exiting...")
		os.Exit(1)
	}

	// Initialize our program
	m := model{}
	p := tea.NewProgram(m)
	if err := p.Start(); err != nil {
		log.Fatal(err)
	}
}

type model struct {
	messages []string
	width    int
	height   int
}

// Init optionally returns an initial command we should run. In this case we
// want to start the timer.
func (m model) Init() tea.Cmd {
	return tea.Batch(tea.EnterAltScreen, tick())
}

// Update is called when messages are received. The idea is that you inspect the
// message and send back an updated model accordingly. You can also return
// a command, which is a function that performs I/O and returns a message.
func (m model) Update(msg tea.Msg) (tea.Model, tea.Cmd) {
	switch msg := msg.(type) {
	case tea.WindowSizeMsg:
		m.width = msg.Width
		m.height = msg.Height
	case tea.KeyMsg:
		switch msg.String() {
		case "ctrl+c", "q", "esc":
			return m, tea.Quit
		}
	case tickMsg:
		var string_array **C.char = C.run_graph_once(graph, &relay_data)
		slice := unsafe.Slice(string_array, relay_data.msg_count)

		if relay_data.msg_count == 0 {
			return m, tick()
		}
		for _, cMsg := range slice {
			goMsg := C.GoString(cMsg)
			if goMsg != "" {
				m.messages = append(m.messages, wordwrap.String(goMsg, min(m.width, maxWidth)))
			}
		}
		return m, tick()
	}
	return m, nil
}

// Views return a string based on data in the model. That string which will be
// rendered to the terminal.
func (m model) View() string {
	return strings.Join(m.messages[:], "\n\n")
}

// Messages are events that we respond to in our Update function. This
// particular one indicates that the timer has ticked.
type tickMsg time.Time

func tick() tea.Cmd {
	return tea.Tick(time.Duration(time.Millisecond), func(t time.Time) tea.Msg {
		return tickMsg(t)
	})
}

func min(a, b int) int {
	if a < b {
		return a
	}
	return b
}
