#!/usr/bin/env bash
./waf --run simple --command-template="%s -tracing -duration=50.0 -prefix_name=reno -transportProt=TcpNewReno" 2> >(tee simple-topology-stderr.txt >&2)
NS_LOG="TcpLearning=all|level_debug" ./waf --run simple --command-template="%s -tracing -duration=50.0 -prefix_name=learning -transportProt=TcpLearning" 2> >(tee simple-topology-stderr.txt >&2)
