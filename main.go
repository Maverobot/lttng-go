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

	"github.com/charmbracelet/bubbles/viewport"
	tea "github.com/charmbracelet/bubbletea"
	"github.com/mattn/go-runewidth"
	"github.com/muesli/reflow/wordwrap"
)

const (
	// You generally won't need this unless you're processing stuff with some
	// pretty complicated ANSI escape sequences. Turn it on if you notice
	// flickering.
	//
	// Also note that high performance rendering only works for programs that
	// use the full size of the terminal. We're enabling that below with
	// tea.EnterAltScreen().
	useHighPerformanceRenderer = false

	headerHeight = 3
	footerHeight = 3
	maxWidth     = 80
)

var (
	relay_data = C.relay_data{}
	graph      *C.bt_graph
	last_key   string
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
	p := tea.NewProgram(m, tea.WithAltScreen(), tea.WithMouseCellMotion())
	if err := p.Start(); err != nil {
		log.Fatal(err)
	}
}

type model struct {
	ready    bool
	messages []string
	width    int
	viewport viewport.Model
}

// Init optionally returns an initial command we should run. In this case we
// want to start the timer.
func (m model) Init() tea.Cmd {
	return tick()
}

// Update is called when messages are received. The idea is that you inspect the
// message and send back an updated model accordingly. You can also return
// a command, which is a function that performs I/O and returns a message.
func (m model) Update(msg tea.Msg) (tea.Model, tea.Cmd) {
	var cmds []tea.Cmd

	switch msg := msg.(type) {
	case tea.WindowSizeMsg:
		m.width = msg.Width
		verticalMargins := headerHeight + footerHeight

		if !m.ready {
			// Since this program is using the full size of the viewport we need
			// to wait until we've received the window dimensions before we
			// can initialize the viewport. The initial dimensions come in
			// quickly, though asynchronously, which is why we wait for them
			// here.
			m.viewport = viewport.Model{Width: msg.Width, Height: msg.Height - verticalMargins}
			m.viewport.HighPerformanceRendering = useHighPerformanceRenderer
			m.ready = true

			// This is only necessary for high performance rendering, which in
			// most cases you won't need.
			//
			// Render the viewport one line below the header.
			m.viewport.YPosition = headerHeight + 1
		} else {
			m.viewport.Width = msg.Width
			m.viewport.Height = msg.Height - verticalMargins
		}

		if useHighPerformanceRenderer {
			// Render (or re-render) the whole viewport. Necessary both to
			// initialize the viewport and when the window is resized.
			//
			// This is needed for high-performance rendering only.
			cmds = append(cmds, viewport.Sync(m.viewport))
		}
	case tea.KeyMsg:
		switch msg.String() {
		case "ctrl+c", "q", "esc":
			return m, tea.Quit
		case "g": // "gg"
			if last_key == "g" {
				m.viewport.GotoTop()
			}
		case "G":
			m.viewport.GotoBottom()
		}
		last_key = msg.String()
	case tickMsg:
		var string_array **C.char = C.run_graph_once(graph, &relay_data)
		slice := unsafe.Slice(string_array, relay_data.msg_count)

		if relay_data.msg_count == 0 {
			cmds = append(cmds, tick())
			break
		}
		for _, cMsg := range slice {
			goMsg := C.GoString(cMsg)
			if goMsg != "" {
				m.messages = append(m.messages, wordwrap.String(goMsg, min(m.width, maxWidth)))
				m.viewport.SetContent(strings.Join(m.messages[:], "\n\n"))
			}
		}
		cmds = append(cmds, tick())
	}
	var cmd tea.Cmd
	m.viewport, cmd = m.viewport.Update(msg)
	if useHighPerformanceRenderer {
		cmds = append(cmds, cmd)
	}

	return m, tea.Batch(cmds...)
}

// Views return a string based on data in the model. That string which will be
// rendered to the terminal.
func (m model) View() string {
	if !m.ready {
		return "\n  Initializing..."
	}

	headerTop := "╭───────────╮"
	headerMid := "│ Mr. Pager ├"
	headerBot := "╰───────────╯"
	headerMid += strings.Repeat("─", m.viewport.Width-runewidth.StringWidth(headerMid))
	header := fmt.Sprintf("%s\n%s\n%s", headerTop, headerMid, headerBot)

	footerTop := "╭──────╮"
	footerMid := fmt.Sprintf("┤ %3.f%% │", m.viewport.ScrollPercent()*100)
	footerBot := "╰──────╯"
	gapSize := m.viewport.Width - runewidth.StringWidth(footerMid)
	footerTop = strings.Repeat(" ", gapSize) + footerTop
	footerMid = strings.Repeat("─", gapSize) + footerMid
	footerBot = strings.Repeat(" ", gapSize) + footerBot
	footer := fmt.Sprintf("%s\n%s\n%s", footerTop, footerMid, footerBot)

	return fmt.Sprintf("%s\n%s\n%s", header, m.viewport.View(), footer)
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
