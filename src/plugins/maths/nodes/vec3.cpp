#include <possumwood_sdk/node_implementation.h>

#include <OpenEXR/ImathVec.h>

#include "io/vec3.h"

namespace {

dependency_graph::InAttr<float> a_x, a_y, a_z;
dependency_graph::OutAttr<Imath::Vec3<float>> a_out;

dependency_graph::State compute(dependency_graph::Values& data) {
	const float x = data.get(a_x);
	const float y = data.get(a_y);
	const float z = data.get(a_z);

	data.set(a_out, Imath::Vec3<float>(x, y, z));

	return dependency_graph::State();
}

void init(possumwood::Metadata& meta) {
	meta.addAttribute(a_x, "x");
	meta.addAttribute(a_y, "y");
	meta.addAttribute(a_z, "z");
	meta.addAttribute(a_out, "out", Imath::Vec3<float>(0, 0, 0));

	meta.addInfluence(a_x, a_out);
	meta.addInfluence(a_y, a_out);
	meta.addInfluence(a_z, a_out);

	meta.setCompute(compute);
}

possumwood::NodeImplementation s_impl("maths/make_vec3", init);

}
