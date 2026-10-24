#!/bin/bash
# Run inside a minimal older-glibc container. The caller provides DISPLAY and
# mounts the repository at /work; no shared Spek/FFmpeg libraries are needed.

set +e
export SPEK_TRACE_STARTUP=1
./build/src/spek tests/samples/2ch-44100Hz-16bps.m4a > old-glibc-program.log 2>&1 &
pid=$!

premature=0
for _ in {1..20}; do
    if ! kill -0 "$pid" 2>/dev/null; then
        wait "$pid"
        status=$?
        echo "The older-glibc GUI exited prematurely with status $status"
        premature=1
        break
    fi
    sleep 0.5
done

if [[ $premature -eq 0 ]]; then
    kill "$pid" 2>/dev/null
    wait "$pid" 2>/dev/null
    echo "The older-glibc GUI stayed alive for ten seconds"
    exit 0
fi

# A failing run is repeated under gdb so the published check contains the
# actual faulting stack rather than only the shell's signal name.
echo "Installing gdb in the diagnostic container"
apt-get update -qq
apt-get install -y -qq --no-install-recommends gdb
if [[ $? -ne 0 ]]; then
    echo "Could not install gdb; the original runtime log follows"
    cat old-glibc-program.log
    exit 1
fi

timeout --signal=TERM --kill-after=5s 25s \
    gdb -q -batch \
    -ex 'set pagination off' \
    -ex run \
    -ex 'thread apply all bt' \
    --args ./build/src/spek tests/samples/2ch-44100Hz-16bps.m4a
status=$?
cat old-glibc-program.log
exit "$status"
