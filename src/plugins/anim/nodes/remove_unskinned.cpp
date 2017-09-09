#include <possumwood_sdk/node_implementation.h>

#include <dependency_graph/values.inl>
#include <dependency_graph/attr.inl>
#include <dependency_graph/datablock.inl>
#include <dependency_graph/port.inl>
#include <dependency_graph/node.inl>

#include <OpenEXR/ImathVec.h>
#include <OpenEXR/ImathEuler.h>

#include "datatypes/skinned_mesh.h"
#include "datatypes/skeleton.h"

namespace {

dependency_graph::InAttr<std::shared_ptr<const std::vector<anim::SkinnedMesh>>> a_inMeshes;
dependency_graph::InAttr<anim::Skeleton> a_inSkel;
dependency_graph::OutAttr<std::shared_ptr<const std::vector<anim::SkinnedMesh>>> a_outMeshes;
dependency_graph::OutAttr<anim::Skeleton> a_outSkel;


dependency_graph::State compute(dependency_graph::Values& data) {
	const auto& inSkel = data.get(a_inSkel);
	const auto& inMeshes = data.get(a_inMeshes);

	if(inMeshes == nullptr || inMeshes->empty() || inSkel.empty()) {
		data.set(a_outSkel, anim::Skeleton());
		data.set(a_outMeshes, std::shared_ptr<const std::vector<anim::SkinnedMesh>>());
	}
	else {
		// collect all skinned bone indices
		std::set<std::size_t> skinnedBones;
		for(auto& mesh : *inMeshes)
			for(auto& vertex : mesh.vertices())
				for(auto& weight : vertex)
					if(weight.second > 0.0f)
						skinnedBones.insert(weight.first);

		// from children to parents (backwards in the hierarchy), add recursively
		//   the parents to the skinned bones set (even if parents themselves were
		//   not skinned)
		for(int boneIndex = inSkel.size()-1; boneIndex > 0; --boneIndex)
			if(skinnedBones.find(boneIndex) != skinnedBones.end())
				skinnedBones.insert(inSkel[boneIndex].parent().index());

		// the resulting skeleton
		anim::Skeleton outSkel;
		outSkel.addRoot(inSkel[0].name(), inSkel[0].tr());

		std::function<void(const anim::Skeleton::Joint& sourceJoint, std::size_t targetJointIndex, anim::Skeleton& target)> convert(
			[&](const anim::Skeleton::Joint& sourceJoint, std::size_t targetJointIndex, anim::Skeleton& target) {
				for(auto& c : sourceJoint.children()) {
					if(skinnedBones.find(c.index()) != skinnedBones.end()) {
						std::size_t bi = target.addChild(target[targetJointIndex], c.tr(), c.name());
						convert(c, bi, target);
					}
				}
			}
		);

		convert(inSkel[0], 0, outSkel);

		data.set(a_outSkel, outSkel);
		data.set(a_outMeshes, inMeshes);
	}

	return dependency_graph::State();
}

void init(possumwood::Metadata& meta) {
	meta.addAttribute(a_inSkel, "in_skeleton");
	meta.addAttribute(a_inMeshes, "in_meshes");
	meta.addAttribute(a_outSkel, "out_skeleton");
	meta.addAttribute(a_outMeshes, "out_meshes");

	meta.addInfluence(a_inMeshes, a_outMeshes);
	meta.addInfluence(a_inSkel, a_outMeshes);
	meta.addInfluence(a_inMeshes, a_outSkel);
	meta.addInfluence(a_inSkel, a_outSkel);

	meta.setCompute(compute);
}

possumwood::NodeImplementation s_impl("anim/remove_unskinned", init);

}