package core

/*
#include <flux/core.h>
#include <flux/idset.h>
#include <flux/hostlist.h>
#include <stddef.h>
#include <jansson.h>
#include <stdlib.h>
#include "../flux/cgo_helpers.h"
*/
import "C"
import (
	"fmt"
	"os"
)

// Create a new Flux Handle
func NewFluxHandle() FluxHandle {

	// Get any FLUX_URI in the environment
	flux_uri := os.Getenv("FLUX_URI")
	uri := C.CString(flux_uri)
	flags := C.int(0)

	// Create the handle
	handle := C.flux_open(uri, flags)

	// I don't know how to catch this if fails
	if handle == nil {
		var err C.flux_error_t
		handle = C.flux_open_ex(uri, flags, &err)
	}

	// if it's still nil, nogo
	if handle == nil {
		fmt.Println("Cannot create Flux handle. Is Flux running?")
		os.Exit(1)
	}
	return FluxHandle{Handle: handle}
}

type FluxHandle struct {
	Handle *C.flux_t
}
