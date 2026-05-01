// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IDummyTransformationSceneNode.h"

namespace scene
{

class CDummyTransformationSceneNode : public IDummyTransformationSceneNode
{
public:
	//! constructor
	CDummyTransformationSceneNode(ISceneNode *parent, ISceneManager *mgr, s32 id);

	//! returns the axis aligned bounding box of this node
	const core::aabbox3d<f32> &getBoundingBox() const override;

	//! Returns a reference to the current relative transformation matrix.
	//! This is the matrix, this scene node uses instead of scale, translation
	//! and rotation.
	core::matrix4 &getRelativeTransformationMatrix() override;

	//! Returns the relative transformation of the scene node.
	core::matrix4 getRelativeTransformation() const override;

	//! does nothing.
	void render() override {}

	//! Returns type of the scene node
	ESCENE_NODE_TYPE getType() const override { return ESNT_DUMMY_TRANSFORMATION; }

	//! Creates a clone of this scene node and its children.
	ISceneNode *clone(ISceneNode *newParent = 0, ISceneManager *newManager = 0) override;

private:
	// NOTE: This node type silently ignores scale/rotation/position setters because its
	// RelativeTransformationMatrix is managed externally (set via getRelativeTransformationMatrix()).
	// The overrides below are intentional no-ops. Bug #2318691 tracked the confusion this causes.
	// If warnings are desired, add warningstream logs in each setter, but note that some scene
	// managers intentionally use this node type for matrix-only transforms.
	const core::vector3df &getScale() const override;
	void setScale(const core::vector3df &scale) override;
	core::vector3df getRotation() const override;
	void setRotation(const core::vector3df &rotation) override;
	const core::vector3df &getPosition() const override;
	void setPosition(const core::vector3df &newpos) override;

	core::matrix4 RelativeTransformationMatrix;
	core::aabbox3d<f32> Box{{0, 0, 0}};
};

} // end namespace scene
