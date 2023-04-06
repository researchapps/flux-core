package main
/*
#include <flux/core.h>
#include <flux/idset.h>
#include <flux/hostlist.h>
#include <stddef.h>
#include <jansson.h>
#include <stdlib.h>
*/
import "C"
import (
	"fmt"
	"github.com/flux-framework/flux-core/pkg/core"
)

func main() {
	fmt.Println("⭐️ Testing flux-core in Go! ⭐️")

	flux := core.NewFlux()
	fmt.Printf("This is a handle: %s\n\n", flux.Handle)

	fmt.Printf("Submitting a Sleep Job: sleep 10\n")

	// Create and submit a jobspec
	jobspec := core.NewJobSpec("sleep 10")
	future := flux.Submit(jobspec)
	fmt.Printf("Flux Future: %s\n", future)

	C.flux_future_wait_all_create()
	//	from _flux._core import ffi, lib

	//	policy := flag.String("policy", "", "Match policy")
	//	label := flag.String("label", "", "Label name for fluence dedicated nodes")

	//	flag.Parse()
	//	flux := fluxion.Fluxion{}
	//	flux.InitFluxion(policy, label)

	//	lis, err := net.Listen("tcp", port)
	//	if err != nil {
	//		fmt.Printf("[GRPCServer] failed to listen: %v\n", err)
	//	}
}