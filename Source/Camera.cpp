/*
Copyright (c) 2022, NVIDIA CORPORATION. All rights reserved.

NVIDIA CORPORATION and its licensors retain all intellectual property
and proprietary rights in and to this software, related documentation
and any modifications thereto. Any use, reproduction, disclosure or
distribution of this software and related documentation without an express
license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "MathLib/MathLib.h"
#include "Timer.h"

#include "Camera.h"

void Camera::Initialize(const float3& position, const float3& lookAt, bool isRelative)
{
    float3 dir = Normalize(lookAt - position);

    float3 rot;
    rot.x = Atan(dir.y, dir.x);
    rot.y = Asin(dir.z);
    rot.z = 0.0f;

    state.globalPosition = ToDouble(position);
    state.rotation = RadToDeg(rot);
    m_IsRelative = isRelative;
}

void Camera::InitializeWithRotation(const float3& position, const float3& rotation, bool isRelative)
{
    state.globalPosition = ToDouble(position);
    state.rotation = rotation;
    m_IsRelative = isRelative;
}

void Camera::Update(const CameraDesc& desc, uint32_t frameIndex)
{
    uint32_t projFlags = desc.isReversedZ ? PROJ_REVERSED_Z : 0;
    projFlags |= desc.isPositiveZ ? PROJ_LEFT_HANDED : 0;

    // Position
    const float3 vRight = state.mWorldToView.GetRow0().To3d();
    const float3 vUp = state.mWorldToView.GetRow1().To3d();
    const float3 vForward = state.mWorldToView.GetRow2().To3d();

    float3 delta = desc.dLocal * desc.timeScale;
    delta.z *= desc.isPositiveZ ? 1.0f : -1.0f;

    state.globalPosition += ToDouble(vRight * delta.x);
    state.globalPosition += ToDouble(vUp * delta.y);
    state.globalPosition += ToDouble(vForward * delta.z);
    state.globalPosition += ToDouble(desc.dUser);

    if (desc.limits.IsValid())
        state.globalPosition = Clamp(state.globalPosition, ToDouble(desc.limits.vMin), ToDouble(desc.limits.vMax));

    if (desc.isCustomMatrixSet)
        state.globalPosition = ToDouble( desc.customMatrix.GetRow3().To3d() );

    if (m_IsRelative)
    {
        state.position = float3::Zero();
        statePrev.position = ToFloat(statePrev.globalPosition - state.globalPosition);
        statePrev.mWorldToView.PreTranslation(-statePrev.position);
    }
    else
    {
        state.position = ToFloat(state.globalPosition);
        statePrev.position = ToFloat(statePrev.globalPosition);
    }

    // Rotation
    float angularSpeed = 0.03f * Saturate( desc.horizontalFov * 0.5f / 90.0f );

    state.rotation.x += desc.dYaw * angularSpeed;
    state.rotation.y += desc.dPitch * angularSpeed;

    state.rotation.x = Mod(state.rotation.x, 360.0f);
    state.rotation.y = Clamp(state.rotation.y, -90.0f, 90.0f);

    if (desc.isCustomMatrixSet)
    {
        state.mViewToWorld = desc.customMatrix;

        state.rotation = RadToDeg( state.mViewToWorld.GetRotationYPR() );
        state.rotation.z = 0.0f;
    }
    else
        state.mViewToWorld.SetupByRotationYPR( DegToRad(state.rotation.x), DegToRad(state.rotation.y), DegToRad(state.rotation.z) );

    state.mWorldToView = state.mViewToWorld;
    state.mWorldToView.PreTranslation( state.mWorldToView.GetRow2().To3d() * desc.backwardOffset );
    state.mWorldToView.WorldToView(projFlags);
    state.mWorldToView.PreTranslation( -state.position );

    // Projection
    if(desc.orthoRange > 0.0f)
    {
        float x = desc.orthoRange;
        float y = desc.orthoRange / desc.aspectRatio;
        state.mViewToClip.SetupByOrthoProjection(-x, x, -y, y, desc.nearZ, desc.farZ, projFlags);
    }
    else
    {
        if (desc.farZ == 0.0f)
            state.mViewToClip.SetupByHalfFovxInf(0.5f * DegToRad(desc.horizontalFov), desc.aspectRatio, desc.nearZ, projFlags);
        else
            state.mViewToClip.SetupByHalfFovx(0.5f * DegToRad(desc.horizontalFov), desc.aspectRatio, desc.nearZ, desc.farZ, projFlags);
    }

    // Other
    state.mWorldToClip = state.mViewToClip * state.mWorldToView;

    state.mViewToWorld = state.mWorldToView;
    state.mViewToWorld.InvertOrtho();

    state.mClipToView = state.mViewToClip;
    state.mClipToView.Invert();

    state.mClipToWorld = state.mWorldToClip;
    state.mClipToWorld.Invert();

    state.viewportJitter = Halton2D( frameIndex ) - 0.5f;

    // Previous other
    statePrev.mWorldToClip = statePrev.mViewToClip * statePrev.mWorldToView;

    statePrev.mViewToWorld = statePrev.mWorldToView;
    statePrev.mViewToWorld.InvertOrtho();

    statePrev.mClipToWorld = statePrev.mWorldToClip;
    statePrev.mClipToWorld.Invert();
}
