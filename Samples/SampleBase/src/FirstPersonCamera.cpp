/*     Copyright 2015-2019 Egor Yusov
 * 
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF ANY PROPRIETARY RIGHTS.
 *
 *  In no event and under no legal theory, whether in tort (including negligence), 
 *  contract, or otherwise, unless required by applicable law (such as deliberate 
 *  and grossly negligent acts) or agreed to in writing, shall any Contributor be
 *  liable for any damages, including any direct, indirect, special, incidental, 
 *  or consequential damages of any character arising as a result of this License or 
 *  out of the use or inability to use the software (including but not limited to damages 
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and 
 *  all other commercial damages or losses), even if such Contributor has been advised 
 *  of the possibility of such damages.
 */

#include "FirstPersonCamera.h"
#include <algorithm>

namespace Diligent
{

void FirstPersonCamera::Update(InputController& Controller, float ElapsedTime)
{
    float3 MoveDirection = float3(0,0,0);
    // Update acceleration vector based on keyboard state
    if (Controller.IsKeyDown(InputKeys::MoveForward))
        MoveDirection.z += 1.0f;
    if (Controller.IsKeyDown(InputKeys::MoveBackward))
        MoveDirection.z -= 1.0f;

    if (Controller.IsKeyDown(InputKeys::MoveRight))
        MoveDirection.x += 1.0f;
    if (Controller.IsKeyDown(InputKeys::MoveLeft))
        MoveDirection.x -= 1.0f;

    if (Controller.IsKeyDown(InputKeys::MoveUp))
        MoveDirection.y += 1.0f;
    if (Controller.IsKeyDown(InputKeys::MoveDown))
        MoveDirection.y -= 1.0f;

    // Normalize vector so if moving in 2 dirs (left & forward), 
    // the camera doesn't move faster than if moving in 1 dir
    auto len = length(MoveDirection);
    if(len != 0.0)
        MoveDirection /= len;

    bool IsSpeedUpScale      = Controller.IsKeyDown(InputKeys::ShiftDown);
    bool IsSuperSpeedUpScale = Controller.IsKeyDown(InputKeys::ControlDown);

    MoveDirection *= m_fMoveSpeed;
    if (IsSpeedUpScale)     MoveDirection *= m_fSpeedUpScale;
    if (IsSuperSpeedUpScale)MoveDirection *= m_fSuperSpeedUpScale;

    m_fCurrentSpeed = length(MoveDirection);

    float3 PosDelta = MoveDirection * ElapsedTime;

    {
        const auto& mouseState = Controller.GetMouseState();
        float MouseDeltaX = 0;
        float MouseDeltaY = 0;
        if (m_LastMouseState.PosX >=0 && m_LastMouseState.PosY >= 0 &&
            m_LastMouseState.ButtonFlags != MouseState::BUTTON_FLAG_NONE)
        {
            MouseDeltaX = mouseState.PosX - m_LastMouseState.PosX;
            MouseDeltaY = mouseState.PosY - m_LastMouseState.PosY;
        }
        m_LastMouseState = mouseState;

        float fYawDelta   = MouseDeltaX * m_fRotationSpeed;
        float fPitchDelta = MouseDeltaY * m_fRotationSpeed;
        if (mouseState.ButtonFlags & MouseState::BUTTON_FLAG_LEFT)
        {
            m_fYawAngle   += fYawDelta;
            m_fPitchAngle += fPitchDelta;
            m_fPitchAngle = std::max(m_fPitchAngle, -PI_F / 2.f);
            m_fPitchAngle = std::min(m_fPitchAngle, +PI_F / 2.f);
        }
    }

    float4x4 ReferenceRotation
    {
            m_ReferenceRightAxis.x,  m_ReferenceUpAxis.x,  m_ReferenceAheadAxis.x,   0,
            m_ReferenceRightAxis.y,  m_ReferenceUpAxis.y,  m_ReferenceAheadAxis.y,   0,
            m_ReferenceRightAxis.z,  m_ReferenceUpAxis.z,  m_ReferenceAheadAxis.z,   0,
                                 0,                    0,                       0,   1
    };

    float4x4 CameraRotation = float4x4::RotationArbitrary(m_ReferenceUpAxis,    -m_fYawAngle) *
                              float4x4::RotationArbitrary(m_ReferenceRightAxis, -m_fPitchAngle) *
                              ReferenceRotation;
    float4x4 WorldRotation = CameraRotation.Transpose();
 
    float3 PosDeltaWorld = PosDelta * WorldRotation;
    m_Pos += PosDeltaWorld;

    m_ViewMatrix = float4x4::Translation(-m_Pos) * CameraRotation;
    m_WorldMatrix = WorldRotation * float4x4::Translation(m_Pos);
}

void FirstPersonCamera::SetReferenceAxes(const float3& ReferenceRightAxis, const float3& ReferenceUpAxis)
{
    m_ReferenceRightAxis = normalize(ReferenceRightAxis);
    m_ReferenceUpAxis    = ReferenceUpAxis - dot(ReferenceUpAxis, m_ReferenceRightAxis) * m_ReferenceRightAxis;
    auto UpLen = length(m_ReferenceUpAxis);
    constexpr float Epsilon = 1e-5f;
    if (UpLen < Epsilon)
    {
        UpLen = Epsilon;
        LOG_WARNING_MESSAGE("Right and Up axes are collinear");
    }
    m_ReferenceUpAxis /= UpLen;

    m_ReferenceAheadAxis = cross(m_ReferenceRightAxis, m_ReferenceUpAxis);
    auto AheadLen = length(m_ReferenceAheadAxis);
    if (AheadLen < Epsilon)
    {
        AheadLen = Epsilon;
        LOG_WARNING_MESSAGE("Ahead axis is not well defined");
    }
    m_ReferenceAheadAxis /= AheadLen;
}

void FirstPersonCamera::SetLookAt(const float3 &LookAt)
{
    float3 ViewDir = LookAt - m_Pos;
    m_fYawAngle = atan2f( ViewDir.x, ViewDir.z );

    float fXZLen = sqrtf( ViewDir.z * ViewDir.z + ViewDir.x * ViewDir.x );
    m_fPitchAngle = -atan2f( ViewDir.y, fXZLen );
}

void FirstPersonCamera::SetRotation(float Yaw, float Pitch)
{
    m_fYawAngle = Yaw;
    m_fPitchAngle = Pitch;
}

void FirstPersonCamera::SetProjAttribs(Float32 NearClipPlane, 
                                       Float32 FarClipPlane, 
                                       Float32 AspectRatio,
                                       Float32 FOV,
                                       bool    IsGL)
{
    m_ProjAttribs.NearClipPlane = NearClipPlane;
    m_ProjAttribs.FarClipPlane  = FarClipPlane;
    m_ProjAttribs.AspectRatio   = AspectRatio;
    m_ProjAttribs.FOV           = FOV;
    m_ProjAttribs.IsGL          = IsGL;

    m_ProjMatrix = float4x4::Projection(m_ProjAttribs.FOV, m_ProjAttribs.AspectRatio, m_ProjAttribs.NearClipPlane, m_ProjAttribs.FarClipPlane, IsGL);
}

void FirstPersonCamera::SetSpeedUpScales(Float32 SpeedUpScale, Float32 SuperSpeedUpScale)
{
    m_fSpeedUpScale = SpeedUpScale;
    m_fSuperSpeedUpScale = SuperSpeedUpScale;
}

}
