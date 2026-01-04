.PHONY: all clean debug release format codesign install test run dev

# default target
all: release

# release build
release:
	@mkdir -p build
	@cd build && cmake -DCMAKE_BUILD_TYPE=Release .. && make
	@echo "✓ Built build/dwin"

# debug build
debug:
	@mkdir -p build
	@cd build && cmake -DCMAKE_BUILD_TYPE=Debug .. && make
	@echo "✓ Built build/dwin (debug)"

# clean build artifacts
clean:
	@rm -rf build
	@echo "✓ Cleaned build artifacts"

# format all source files
format:
	@./scripts/format.sh

# self-sign the binary (accessibility permissions)
codesign: release
	@./scripts/codesign.sh

# install to /Applications
install: codesign
	@./scripts/install.sh

# run tests
test:
	@mkdir -p build
	@cd build && cmake -DBUILD_TESTS=ON .. && make && ctest --output-on-failure
	@echo "✓ Ran tests"

# run the app (build + codesign + launch detached)
run: debug
	@codesign --force --deep --sign - build/dwin.app
	@echo "✓ Signed build/dwin.app"
	@open build/dwin.app
	@echo "✓ Launched dwin.app"

# dev mode (build + codesign + run with logs, Ctrl+C to quit)
dev: debug
	@codesign --force --deep --sign - build/dwin.app
	@echo "✓ Signed build/dwin.app"
	@echo "✓ Running dwin.app (Ctrl+C to quit)..."
	@build/dwin.app/Contents/MacOS/dwin

# help
help:
	@echo "dwin build targets:"
	@echo "  make          - Build release binary"
	@echo "  make debug    - Build with debug symbols"
	@echo "  make dev      - Build + run with logs (Ctrl+C to quit)"
	@echo "  make run      - Build + launch detached"
	@echo "  make clean    - Remove build artifacts"
	@echo "  make format   - Format all source files"
	@echo "  make codesign - Sign the binary"
	@echo "  make install  - Install to /Applications"
	@echo "  make test     - Run unit tests"