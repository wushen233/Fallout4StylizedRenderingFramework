#include "Render/FaceTransportPublicationContract.h"

#include <array>
#include <cassert>

int main()
{
	using cs::engine::face_transport::CanConsume;
	using cs::engine::face_transport::CanPublish;

	assert(CanPublish(true, true, true, true, true));
	for (std::size_t failed = 0; failed < 5; ++failed) {
		std::array<bool, 5> facts{ true, true, true, true, true };
		facts[failed] = false;
		assert(!CanPublish(facts[0], facts[1], facts[2], facts[3], facts[4]));
	}
	assert(!CanConsume(true, true, true, false, true, true));  // View identity absent.
	assert(!CanConsume(true, true, true, true, false, true));  // Unsupported writer.
	assert(!CanConsume(true, true, true, true, true, false));  // Candidate disabled.
	assert(CanConsume(true, true, true, true, true, true));
}
