//! @file profiling.c
//! @brief Implementation of mcount function

//! @brief Profiler hook called on each function call (except calls to mcount of course)
void mcount() {
	// TOOD: Actually profile.
	// Things to do here:
	// 1) Check if cpu locals are initialized
	// 2) If they are, query CPU id, otherwise assume BSP
	// 3) guard against situations when we are on AP but locals are not initialized
	// 4) Get return address and return address from the caller
	// 5) Maybe add file backed memory object in the future
}
