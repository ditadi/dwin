.PHONY: all clean debug release format codesign install test

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

# help
help:
	@echo "dwin build targets:"
	@echo "  make          - Build release binary"
	@echo "  make debug    - Build with debug symbols"
	@echo "  make clean    - Remove build artifacts"
	@echo "  make format   - Format all source files"
	@echo "  make codesign - Sign the binary"
	@echo "  make install  - Install to /Applications"
	@echo "  make test     - Run unit tests"