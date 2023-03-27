package main

import (
	"fmt"
	"github.com/flux-framework/flux-core/pkg/core"
)


func main () {
	fmt.Println("⭐️ Testing flux-core in Go! ⭐️")

	flux := core.NewFluxHandle()
	fmt.Printf("This is a handle: %s\n\n", flux.Handle)

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