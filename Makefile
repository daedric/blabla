all:
	${MAKE} -C build all

clean:
	${MAKE} -C build clean

cmake:
	rm -rf build
	mkdir build
	cd build && cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DCMAKE_BUILD_TYPE=Debug -DSANITIZE_ADDRESS=ON ..

relcmake:
	rm -rf build
	mkdir build
	cd build && cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DCMAKE_BUILD_TYPE=RelWithDebInfo ..

test:
	${MAKE} -C build test

re : cmake all

format:
	${MAKE} -C build format

.PHONY: cmake test format

