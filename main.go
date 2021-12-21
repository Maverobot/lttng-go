package main

/*
   #cgo pkg-config: babeltrace2
   #cgo LDFLAGS: -L. -lbabeltrace2
   #include <babeltrace2/babeltrace.h>
   #include <read_live_stream.h>
*/
import "C"
import "os"

func main() {
	C.listenToLttngLive(C.CString(os.Args[1]))
}
