#pragma once


class Object;
class Vector3f;
class Quaternionf;

Object& InstantiateObjectRemoveAllNonAnimationComponents (Object& inObject, const Vector3f& worldPos, const Quaternionf& worldRot);
