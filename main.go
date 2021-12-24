package main

/*
   #cgo pkg-config: babeltrace2
   #cgo LDFLAGS: -L. -lbabeltrace2 -ljson-c
   #include <read_live_stream.h>
*/
import "C"

import (
	"log"
	"os"
	"time"
	"unsafe"

	"github.com/charmbracelet/bubbles/list"
	tea "github.com/charmbracelet/bubbletea"
	"github.com/charmbracelet/lipgloss"
	"github.com/goccy/go-json"
)

var (
	relay_data = C.relay_data{}
	graph      *C.bt_graph

	appStyle   = lipgloss.NewStyle().Padding(1, 2)
	titleStyle = lipgloss.NewStyle().
			Foreground(lipgloss.Color("#FFFDF5")).
			Background(lipgloss.Color("#25A065")).
			Padding(0, 2)
	paginationStyle = list.DefaultStyles().PaginationStyle.PaddingLeft(4)
	helpStyle       = list.DefaultStyles().HelpStyle.PaddingLeft(4).PaddingBottom(1)
)

func main() {
	/* Set up logger file*/
	path := os.Getenv("LTTNG_GO_LOG")
	if path == "" {
		path = "/tmp/lttng-go.log"
	}

	f, err := tea.LogToFile(path, "lttng-go")
	if err != nil {
		log.Fatalf("Could not open file %s: %v", path, err)
		os.Exit(1)
	}
	defer f.Close()

	/* Create the trace processing graph */
	url := C.CString(os.Args[1])
	defer C.free(unsafe.Pointer(url))
	graph = C.create_graph(url, &relay_data)
	defer C.bt_graph_put_ref(graph)
	if graph == nil {
		log.Fatalf("No graph can be created. Exiting...")
		os.Exit(1)
	}

	// Initialize our program
	const defaultWidth = 20
	const listHeight = 14
	l := list.NewModel([]list.Item{}, newLttngDelegate(), defaultWidth, listHeight)
	l.Title = "lttng-go"
	l.SetShowStatusBar(false)
	l.SetFilteringEnabled(true)
	l.SetShowPagination(true)
	l.Styles.Title = titleStyle
	l.Styles.PaginationStyle = paginationStyle
	l.Styles.HelpStyle = helpStyle
	m := model{list: l}
	p := tea.NewProgram(m)
	if err := p.Start(); err != nil {
		log.Fatal(err)
	}
}

type message map[string]interface{}

type model struct {
	list     list.Model
	messages []list.Item
	width    int
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
	var cmds []tea.Cmd

	switch msg := msg.(type) {
	case tea.WindowSizeMsg:
		m.width = msg.Width
		topGap, rightGap, bottomGap, leftGap := appStyle.GetPadding()
		m.list.SetSize(msg.Width-leftGap-rightGap, msg.Height-topGap-bottomGap)
	case tea.KeyMsg:
		switch keypress := msg.String(); keypress {
		case "ctrl+c":
			return m, tea.Quit
		case "ctrl+j":
			m.list.CursorDown()
		case "ctrl+k":
			m.list.CursorUp()
		default:
			if !m.list.SettingFilter() && (keypress == "q") {
				return m, tea.Quit
			}
		}
	case tickMsg:
		var string_array **C.char = C.run_graph_once(graph, &relay_data)
		slice := unsafe.Slice(string_array, relay_data.msg_count)

		if relay_data.msg_count == 0 {
			return m, tick()
		}
		hasValidMsg := false
		for _, cMsg := range slice {
			goMsg := C.GoString(cMsg)
			if goMsg != "" {
				hasValidMsg = true
				var data message
				err := json.Unmarshal([]byte(goMsg), &data)
				if err != nil {
					// TODO: log the warning
					panic(err)
				}

				payload, _ := json.Marshal(data["payload"])
				m.messages = append(m.messages, item{
					title:       data["name"].(string),
					description: string(payload),
				})
			}
		}
		if hasValidMsg {
			m.list.SetItems(m.messages)
			m.list.Paginator.Page = m.list.Paginator.TotalPages - 1
			m.list.Select(len(m.list.Items()) - 1)
		}
		cmds = append(cmds, tick())

	}
	var cmd tea.Cmd
	m.list, cmd = m.list.Update(msg)
	cmds = append(cmds, cmd)

	return m, tea.Batch(cmds...)
}

// Views return a string based on data in the model. That string which will be
// rendered to the terminal.
func (m model) View() string {
	return appStyle.Render(m.list.View())
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
