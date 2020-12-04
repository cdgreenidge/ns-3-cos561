#!/usr/bin/env bash
NS_LOG="TcpLearning=all|level_debug" ./waf --run simple --command-template="%s -tracing -duration=5.0 -transportProt=TcpLearning" 2> >(tee simple-topology-stderr.txt >&2)
