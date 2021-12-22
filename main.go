package main

/*
   #cgo pkg-config: babeltrace2
   #cgo LDFLAGS: -L. -lbabeltrace2 -ljson-c
   #include <read_live_stream.h>
*/
import "C"
import (
	"fmt"
	"os"
	"os/signal"
	"unsafe"
)

func main() {
	relay_data := C.relay_data{}

	/* Create the trace processing graph */
	url := C.CString(os.Args[1])
	defer C.free(unsafe.Pointer(url))
	graph := C.create_graph(url, &relay_data)
	defer C.bt_graph_put_ref(graph)
	if graph == nil {
		fmt.Fprintf(os.Stderr, "No graph can be created. Exiting...")
		os.Exit(1)
	}

	/* Cleanup on Ctrl+C */
	c := make(chan os.Signal, 1)
	signal.Notify(c, os.Interrupt)
	go func() {
		<-c
		C.free(unsafe.Pointer(url))
		C.bt_graph_put_ref(graph)
		fmt.Println("Aborting...")
		os.Exit(1)
	}()

	for {
		var string_array **C.char = C.run_graph_once(graph, &relay_data)
		slice := unsafe.Slice(string_array, relay_data.msg_count)

		if relay_data.msg_count == 0 {
			continue
		}
		for _, cMsg := range slice {
			goMsg := C.GoString(cMsg)
			if goMsg != "" {
				fmt.Printf("%v\n", goMsg)
			}
		}
	}
}
