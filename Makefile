.PHONY: all linux macos windows android wasm test clean dist

all:
	./scripts/build.sh all

linux:
	./scripts/build.sh linux

macos:
	./scripts/build.sh macos

windows:
	./scripts/build.sh windows

android:
	./scripts/build.sh android

wasm:
	./scripts/build.sh wasm

test:
	./scripts/build.sh test

clean:
	rm -rf build dist build-* 

dist:
	@ls -la dist/ 2>/dev/null || echo "No dist/ yet — run: make linux"
