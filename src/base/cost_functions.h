// by cheng.li
// Author: Johannes L. Schoenberger (jsch-at-demuc-dot-de)

#ifndef COLMAP_SRC_BASE_COST_FUNCTIONS_H_
#define COLMAP_SRC_BASE_COST_FUNCTIONS_H_

#include <Eigen/Core>

#include <ceres/ceres.h>
#include <ceres/rotation.h>
#include <base/pose.h>
namespace colmap {

typedef  Eigen::Vector3d Vec3;
// Standard bundle adjustment cost function for variable
// camera pose and calibration and point parameters.
template <typename CameraModel>
class BundleAdjustmentCostFunction {
 public:
  explicit BundleAdjustmentCostFunction(const Eigen::Vector2d& point2D)
      : observed_x_(point2D(0)), observed_y_(point2D(1)) {}

  static ceres::CostFunction* Create(const Eigen::Vector2d& point2D) {
    return (new ceres::AutoDiffCostFunction<
            BundleAdjustmentCostFunction<CameraModel>, 2, 4, 3, 3,
            CameraModel::kNumParams>(
        new BundleAdjustmentCostFunction(point2D)));
  }

  template <typename T>
  bool operator()(const T* const qvec, const T* const tvec,
                  const T* const point3D, const T* const camera_params,
                  T* residuals) const {
    // Rotate and translate.
    T projection[3];
    ceres::UnitQuaternionRotatePoint(qvec, point3D, projection);
    projection[0] += tvec[0];
    projection[1] += tvec[1];
    projection[2] += tvec[2];

    // Project to image plane.
    projection[0] /= projection[2];
    projection[1] /= projection[2];

    // Distort and transform to pixel space.
    CameraModel::WorldToImage(camera_params, projection[0], projection[1],
                              &residuals[0], &residuals[1]);

    // Re-projection error.
    residuals[0] -= T(observed_x_);
    residuals[1] -= T(observed_y_);

    return true;
  }

 private:
  const double observed_x_;
  const double observed_y_;
  // 利用特征点的scale 定投影误差的权
};

// add gps position constraint,licheng
struct PoseCenterConstraintCostFunction
{
    Vec3  weight_;
    Vec3 pose_center_constraint;
    PoseCenterConstraintCostFunction
    (
       const Vec3 & center,
       const Vec3 & weight
    ):weight_(weight),pose_center_constraint(center){}

    static ceres::CostFunction* Create(const Vec3 & center,const Vec3 & weight) {
      return (new ceres::AutoDiffCostFunction<
              PoseCenterConstraintCostFunction, 3, 4,3>(
          new PoseCenterConstraintCostFunction(center,weight)));
    }

    template <typename T>
    bool operator()(const T* const qvec, const T* const tvec,
                    T* residuals) const
    {

      T qvec_inv[4]; // 取逆
      qvec_inv[0]=qvec[0];
      qvec_inv[1]=-qvec[1];
      qvec_inv[2]=-qvec[2];
      qvec_inv[3]=-qvec[3];
      T pose_center[3];
      ceres::UnitQuaternionRotatePoint(qvec_inv, tvec, pose_center);
      using Vec3T = Eigen::Matrix<T,3,1>;
      Vec3T pose_center_;
      pose_center_(0)=T(-1)*pose_center[0];
      pose_center_(1)=T(-1)*pose_center[1];
      pose_center_(2)=T(-1)*pose_center[2];

      Eigen::Map<Vec3T> residuals_eigen(residuals);
      residuals_eigen=weight_.cast<T>().cwiseProduct(pose_center_-pose_center_constraint.cast<T>());
      return true;

    }
};
template <typename CameraModel>
class BundleAdjustmentConstantPoseCostFunction {
 public:
  BundleAdjustmentConstantPoseCostFunction(const Eigen::Vector4d& qvec,
                                           const Eigen::Vector3d& tvec,
                                           const Eigen::Vector2d& point2D)
      : qw_(qvec(0)),
        qx_(qvec(1)),
        qy_(qvec(2)),
        qz_(qvec(3)),
        tx_(tvec(0)),
        ty_(tvec(1)),
        tz_(tvec(2)),
        observed_x_(point2D(0)),
        observed_y_(point2D(1)) {}

  static ceres::CostFunction* Create(const Eigen::Vector4d& qvec,
                                     const Eigen::Vector3d& tvec,
                                     const Eigen::Vector2d& point2D) {
    return (new ceres::AutoDiffCostFunction<
            BundleAdjustmentConstantPoseCostFunction<CameraModel>, 2, 3,
            CameraModel::kNumParams>(
        new BundleAdjustmentConstantPoseCostFunction(qvec, tvec, point2D)));
  }

  template <typename T>
  bool operator()(const T* const point3D, const T* const camera_params,
                  T* residuals) const {
    const T qvec[4] = {T(qw_), T(qx_), T(qy_), T(qz_)};

    // Rotate and translate.
    T projection[3];
    ceres::UnitQuaternionRotatePoint(qvec, point3D, projection);
    projection[0] += T(tx_);
    projection[1] += T(ty_);
    projection[2] += T(tz_);

    // Project to image plane.
    projection[0] /= projection[2];
    projection[1] /= projection[2];

    // Distort and transform to pixel space.
    CameraModel::WorldToImage(camera_params, projection[0], projection[1],
                              &residuals[0], &residuals[1]);

    // Re-projection error.
    residuals[0] -= T(observed_x_);
    residuals[1] -= T(observed_y_);

    return true;
  }

 private:
  const double qw_;
  const double qx_;
  const double qy_;
  const double qz_;
  const double tx_;
  const double ty_;
  const double tz_;
  const double observed_x_;
  const double observed_y_;
};

// Rig bundle adjustment cost function for variable camera pose and calibration
// and point parameters. Different from the standard bundle adjustment function,
// this cost function is suitable for camera rigs with consistent relative poses
// of the cameras within the rig. The cost function first projects points into
// the local system of the camera rig and then into the local system of the
// camera within the rig.
template <typename CameraModel>
class RigBundleAdjustmentCostFunction {
 public:
  explicit RigBundleAdjustmentCostFunction(const Eigen::Vector2d& point2D)
      : observed_x_(point2D(0)), observed_y_(point2D(1)) {}

  static ceres::CostFunction* Create(const Eigen::Vector2d& point2D) {
    return (new ceres::AutoDiffCostFunction<
            RigBundleAdjustmentCostFunction<CameraModel>, 2, 4, 3, 4, 3, 3,
            CameraModel::kNumParams>(
        new RigBundleAdjustmentCostFunction(point2D)));
  }

  template <typename T>
  bool operator()(const T* const rig_qvec, const T* const rig_tvec,
                  const T* const rel_qvec, const T* const rel_tvec,
                  const T* const point3D, const T* const camera_params,
                  T* residuals) const {
    // Concatenate rotations.
    T qvec[4];
    ceres::QuaternionProduct(rel_qvec, rig_qvec, qvec);

    // Concatenate translations.
    T tvec[3];
    ceres::UnitQuaternionRotatePoint(rel_qvec, rig_tvec, tvec);
    tvec[0] += rel_tvec[0];
    tvec[1] += rel_tvec[1];
    tvec[2] += rel_tvec[2];

    // Rotate and translate.
    T projection[3];
    ceres::UnitQuaternionRotatePoint(qvec, point3D, projection);
    projection[0] += tvec[0];
    projection[1] += tvec[1];
    projection[2] += tvec[2];

    // Project to image plane.
    projection[0] /= projection[2];
    projection[1] /= projection[2];

    // Distort and transform to pixel space.
    CameraModel::WorldToImage(camera_params, projection[0], projection[1],
                              &residuals[0], &residuals[1]);

    // Re-projection error.
    residuals[0] -= T(observed_x_);
    residuals[1] -= T(observed_y_);

    return true;
  }

 private:
  const double observed_x_;
  const double observed_y_;
};

// Cost function for refining two-view geometry based on the Sampson-Error.
//
// First pose is assumed to be located at the origin with 0 rotation. Second
// pose is assumed to be on the unit sphere around the first pose, i.e. the
// pose of the second camera is parameterized by a 3D rotation and a
// 3D translation with unit norm. `tvec` is therefore over-parameterized as is
// and should be down-projected using `HomogeneousVectorParameterization`.
class RelativePoseCostFunction {
 public:
  RelativePoseCostFunction(const Eigen::Vector2d& x1, const Eigen::Vector2d& x2)
      : x1_(x1(0)), y1_(x1(1)), x2_(x2(0)), y2_(x2(1)) {}

  static ceres::CostFunction* Create(const Eigen::Vector2d& x1,
                                     const Eigen::Vector2d& x2) {
    return (new ceres::AutoDiffCostFunction<RelativePoseCostFunction, 1, 4, 3>(
        new RelativePoseCostFunction(x1, x2)));
  }

  template <typename T>
  bool operator()(const T* const qvec, const T* const tvec,
                  T* residuals) const {
    Eigen::Matrix<T, 3, 3, Eigen::RowMajor> R;
    ceres::QuaternionToRotation(qvec, R.data());

    // Matrix representation of the cross product t x R.
    Eigen::Matrix<T, 3, 3> t_x;
    t_x << T(0), -tvec[2], tvec[1], tvec[2], T(0), -tvec[0], -tvec[1], tvec[0],
        T(0);

    // Essential matrix.
    const Eigen::Matrix<T, 3, 3> E = t_x * R;

    // Homogeneous image coordinates.
    const Eigen::Matrix<T, 3, 1> x1_h(T(x1_), T(y1_), T(1));
    const Eigen::Matrix<T, 3, 1> x2_h(T(x2_), T(y2_), T(1));

    // Squared sampson error.
    const Eigen::Matrix<T, 3, 1> Ex1 = E * x1_h;
    const Eigen::Matrix<T, 3, 1> Etx2 = E.transpose() * x2_h;
    const T x2tEx1 = x2_h.transpose() * Ex1;
    residuals[0] = x2tEx1 * x2tEx1 /
                   (Ex1(0) * Ex1(0) + Ex1(1) * Ex1(1) + Etx2(0) * Etx2(0) +
                    Etx2(1) * Etx2(1));

    return true;
  }

 private:
  const double x1_;
  const double y1_;
  const double x2_;
  const double y2_;
};

}  // namespace colmap

#endif  // COLMAP_SRC_BASE_COST_FUNCTIONS_H_
