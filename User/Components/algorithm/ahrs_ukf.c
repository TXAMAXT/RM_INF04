//
// Created by Ken_n on 2022/10/1.
//

#include "ahrs_ukf.h"
#include "matrix.h"
#include "INS_task.h"
#include "global_control_define.h"

//new_AHRS_define
float32_t RLS_lambda = 0.999999f;/* Forgetting factor */
matrix_f32_t RLS_theta;/* The variables we want to indentify */
matrix_f32_t RLS_P;/* Inverse of correction estimation */
matrix_f32_t RLS_in;/* Input data */
matrix_f32_t RLS_out;/* Output data */
matrix_f32_t RLS_gain;/* RLS gain */
uint32_t RLS_u32iterData = 0;/* To track how much data we take */

/* P(k=0) variable --------------------------------------------------------------------------------------------------------- */
float32_t UKF_PINIT_data[SS_X_LEN * SS_X_LEN] = {P_INIT, 0, 0, 0,
                                                 0, P_INIT, 0, 0,
                                                 0, 0, P_INIT, 0,
                                                 0, 0, 0, P_INIT};
matrix_f32_t UKF_PINIT;
/* Q constant -------------------------------------------------------------------------------------------------------------- */
float UKF_Rv_data[SS_X_LEN * SS_X_LEN] = {Rv_INIT, 0, 0, 0,
                                          0, Rv_INIT, 0, 0,
                                          0, 0, Rv_INIT, 0,
                                          0, 0, 0, Rv_INIT};
matrix_f32_t UKF_Rv;
/* R constant -------------------------------------------------------------------------------------------------------------- */
float32_t UKF_Rn_data[SS_Z_LEN * SS_Z_LEN] = {Rn_INIT_ACC, 0, 0, 0, 0, 0,
                                              0, Rn_INIT_ACC, 0, 0, 0, 0,
                                              0, 0, Rn_INIT_ACC, 0, 0, 0,
                                              0, 0, 0, Rn_INIT_MAG, 0, 0,
                                              0, 0, 0, 0, Rn_INIT_MAG, 0,
                                              0, 0, 0, 0, 0, Rn_INIT_MAG};
matrix_f32_t UKF_Rn;
/* UKF variables ----------------------------------------------------------------------------------------------------------- */
matrix_f32_t quaternionData;
matrix_f32_t Y;
matrix_f32_t U;

UKF_t UKF_IMU;

/* =============================================== Sharing Variables/function declaration =============================================== */
//X北向，北东地对前右下
/* Magnetic vector constant (align with local magnetic vector) */
//float32_t IMU_MAG_B0_data[3] = {COS(0), SIN(0), 0.000000f};
float32_t IMU_MAG_B0_data[3] = {North_Comp / Total_Field, East_Comp / Total_Field,
                                Vertical_Comp /
                                Total_Field};//由经纬度和海拔从WMM世界磁场模型计算获得，需旋转并修正磁偏角(need True north magnetic vector),并归一化
/* The hard-magnet bias */
float32_t HARD_IRON_BIAS_data[3] = {0.0f, 0.0f, 0.0f};


void UKF_init(UKF_t *UKF_op, matrix_f32_t *XInit, matrix_f32_t *P, matrix_f32_t *Rv, matrix_f32_t *Rn,
              bool (*bNonlinearUpdateX)(matrix_f32_t *, matrix_f32_t *, matrix_f32_t *, AHRS_t *),
              bool (*bNonlinearUpdateY)(matrix_f32_t *, matrix_f32_t *, matrix_f32_t *, AHRS_t *)) {
    /* Initialization:
 *  P (k=0|k=0) = Identitas * covariant(P(k=0)), typically initialized with some big number.
 *  x(k=0|k=0)  = Expected value of x at time-0 (i.e. x(k=0)), typically set to zero.
 *  Rv, Rn      = Covariance matrices of process & measurement. As this implementation
 *                the noise as AWGN (and same value for every variable), this is set
 *                to Rv=diag(RvInit,...,RvInit) and Rn=diag(RnInit,...,RnInit).
 */
    UKF_op->X_Est = *XInit;
    Matrix_vCopy_f32(&UKF_op->P, P);
    Matrix_vCopy_f32(&UKF_op->Rv, Rv);
    Matrix_vCopy_f32(&UKF_op->Rn, Rn);
    UKF_op->AHRS_bUpdateNonlinearX = bNonlinearUpdateX;
    UKF_op->AHRS_bUpdateNonlinearY = bNonlinearUpdateY;
    Matrix_nodata_creat_f32(&UKF_op->X_Est, SS_X_LEN, 1, InitMatWithZero);
    Matrix_nodata_creat_f32(&UKF_op->X_Sigma, SS_X_LEN, (2 * SS_X_LEN + 1), InitMatWithZero);

    Matrix_nodata_creat_f32(&UKF_op->Y_Est, SS_Z_LEN, 1, InitMatWithZero);
    Matrix_nodata_creat_f32(&UKF_op->Y_Sigma, SS_Z_LEN, (2 * SS_X_LEN + 1), InitMatWithZero);

//    Matrix_nodata_creat_f32(&UKF_op->P, SS_X_LEN, SS_X_LEN, InitMatWithZero);
    Matrix_nodata_creat_f32(&UKF_op->P_Chol, SS_X_LEN, SS_X_LEN, InitMatWithZero);

    Matrix_nodata_creat_f32(&UKF_op->DX, SS_X_LEN, (2 * SS_X_LEN + 1), InitMatWithZero);
    Matrix_nodata_creat_f32(&UKF_op->DY, SS_Z_LEN, (2 * SS_X_LEN + 1), InitMatWithZero);

    Matrix_nodata_creat_f32(&UKF_op->Py, SS_Z_LEN, SS_Z_LEN, InitMatWithZero);
    Matrix_nodata_creat_f32(&UKF_op->Pxy, SS_X_LEN, SS_Z_LEN, InitMatWithZero);

    Matrix_nodata_creat_f32(&UKF_op->Wm, 1, (2 * SS_X_LEN + 1), InitMatWithZero);
//    Matrix_nodata_creat_f32(&UKF_op->Wc, 1, (2 * SS_X_LEN + 1), InitMatWithZero);

//    Matrix_nodata_creat_f32(&UKF_op->Rv, SS_X_LEN, SS_X_LEN, InitMatWithZero);
//    Matrix_nodata_creat_f32(&UKF_op->Rn, SS_Z_LEN, SS_Z_LEN, InitMatWithZero);

    Matrix_nodata_creat_f32(&UKF_op->Err, SS_Z_LEN, 1, InitMatWithZero);
    Matrix_nodata_creat_f32(&UKF_op->Gain, SS_X_LEN, SS_Z_LEN, InitMatWithZero);

    /* Van der. Merwe, .. (2004). Sigma-Point Kalman Filters for Probabilistic Inference in Dynamic State-Space Models
 * (Ph.D. thesis). Oregon Health & Science University. Page 6:
 *
 * where λ = α2(L+κ)−L is a scaling parameter. α determines the spread of the sigma points around ̄x and is usually
 * set to a small positive value (e.g. 1e−2 ≤ α ≤ 1). κ is a secondary scaling parameter which is usually set to either
 * 0 or 3−L (see [45] for details), and β is an extra degree of freedom scalar parameter used to incorporate any extra
 * prior knowledge of the distribution of x (for Gaussian distributions, β = 2 is optimal).
 */
    float32_t _alpha = 1e-2f;
    float32_t _k = 0.0f;
    float32_t _beta = 2.0f;

    /* lambda = (alpha^2)*(N+kappa)-N,         gamma = sqrt(N+alpha)            ...{UKF_1} */
    float32_t _lambda = (_alpha * _alpha) * (SS_X_LEN + _k) - SS_X_LEN;
    UKF_op->Gamma = sqrtf((SS_X_LEN + _lambda));


    /* Wm = [lambda/(N+lambda)         1/(2(N+lambda)) ... 1/(2(N+lambda))]     ...{UKF_2} */
    UKF_op->Wm.p2Data[0][0] = _lambda / (SS_X_LEN + _lambda);
    for (int16_t _i = 1; _i < UKF_op->Wm.arm_matrix.numCols; _i++) {
        UKF_op->Wm.p2Data[0][_i] = 0.5f / (SS_X_LEN + _lambda);
    }

    /* Wc = [Wm(0)+(1-alpha(^2)+beta)  1/(2(N+lambda)) ... 1/(2(N+lambda))]     ...{UKF_3} */
    Matrix_vCopy_f32(&UKF_op->Wm, &UKF_op->Wc);
    UKF_op->Wc.p2Data[0][0] = UKF_op->Wc.p2Data[0][0] + (1.0f - (_alpha * _alpha) + _beta);
}

bool UKF_bUpdate(UKF_t *UKF_op, matrix_f32_t *Y_matrix, matrix_f32_t *U_matrix, AHRS_t *AHRS_op) {
    matrix_f32_t _temp_T;
    matrix_f32_t _temp_M;
    matrix_f32_t _temp_M2;
    Matrix_vinit_f32(&_temp_T);
    Matrix_vinit_f32(&_temp_M);
    Matrix_vinit_f32(&_temp_M2);
    /* Run once every sampling time */
    /* XSigma(k-1) = [x(k-1) Xs(k-1)+GPsq Xs(k-1)-GPsq]                     ...{UKF_4}  */
    if (!UKF_bCalculateSigmaPoint(UKF_op)) {
        return false;
    }

    /* Unscented Transform XSigma [f,XSigma,u,Rv] -> [x,XSigma,P,DX]:       ...{UKF_5a} - {UKF_8a} */
    if (!UKF_bUnscentedTransform(UKF_op, &UKF_op->X_Est, &UKF_op->X_Sigma, &UKF_op->P, &UKF_op->DX,
                                 UKF_op->AHRS_bUpdateNonlinearX, &UKF_op->X_Sigma, U_matrix, &UKF_op->Rv, AHRS_op)) {
        return false;
    }
    /* Unscented Transform YSigma [h,XSigma,u,Rn] -> [y_est,YSigma,Py,DY]:  ...{UKF_5b} - {UKF_8b} */
    if (!UKF_bUnscentedTransform(UKF_op, &UKF_op->Y_Est, &UKF_op->Y_Sigma, &UKF_op->Py, &UKF_op->DY,
                                 UKF_op->AHRS_bUpdateNonlinearY, &UKF_op->X_Sigma, U_matrix, &UKF_op->Rn, AHRS_op)) {
        return false;
    }

    /* Calculate Cross-Covariance Matrix:
     *  Pxy(k) = sum(Wc(i)*DX*DY(i))            ; i = 1 ... (2N+1)          ...{UKF_9}
     */
    for (int16_t _i = 0; _i < UKF_op->DX.arm_matrix.numRows; _i++) {
        for (int16_t _j = 0; _j < UKF_op->DX.arm_matrix.numCols; _j++) {
            UKF_op->DX.p2Data[_i][_j] *= UKF_op->Wc.p2Data[0][_j];
        }
    }
    Matrix_vTranspose_nsame_f32(&UKF_op->DY, &_temp_T);
    Matrix_vmult_nsame_f32(&UKF_op->DX, &_temp_T, &UKF_op->Pxy);

    /* Calculate the Kalman Gain:
     *  K           = Pxy(k) * (Py(k)^-1)                                   ...{UKF_10}
     */
    matrix_f32_t PyInv;
    Matrix_vinit_f32(&PyInv);
    Matrix_vInverse_nsame_f32(&UKF_op->Py, &PyInv);
    if (!Matrix_bMatrixIsValid_f32(&PyInv)) {
        Matrix_vSetMatrixInvalid_f32(&PyInv);
        return false;
    }
    Matrix_vmult_nsame_f32(&UKF_op->Pxy, &PyInv, &UKF_op->Gain);


    /* Update the Estimated State Variable:
     *  x(k|k)      = x(k|k-1) + K * (y(k) - y_est(k))                      ...{UKF_11}
     */
    Matrix_vsub_f32(Y_matrix, &UKF_op->Y_Est, &UKF_op->Err);
    Matrix_vmult_nsame_f32(&UKF_op->Gain, &UKF_op->Err, &_temp_M);
    Matrix_vadd_f32(&UKF_op->X_Est, &_temp_M, &UKF_op->X_Est);

    /* Update the Covariance Matrix:
     *  P(k|k)      = P(k|k-1) - K*Py(k)*K'                                 ...{UKF_12}
     */
    Matrix_vTranspose_nsame_f32(&UKF_op->Gain, &_temp_T);
    Matrix_vmult_nsame_f32(&UKF_op->Gain, &UKF_op->Py, &_temp_M);
    Matrix_vmult_nsame_f32(&_temp_M, &_temp_T, &_temp_M2);
    Matrix_vsub_f32(&UKF_op->P, &_temp_M2, &UKF_op->P);
    Matrix_vSetMatrixInvalid_f32(&PyInv);
    Matrix_vSetMatrixInvalid_f32(&_temp_T);
    Matrix_vSetMatrixInvalid_f32(&_temp_M);
    Matrix_vSetMatrixInvalid_f32(&_temp_M2);

    return true;
}

void UKF_vReset(UKF_t *UKF_op, matrix_f32_t *XInit, matrix_f32_t *P, matrix_f32_t *Rv, matrix_f32_t *Rn) {
    UKF_op->X_Est = *XInit;
    Matrix_vCopy_f32(&UKF_op->P, P);
    Matrix_vCopy_f32(&UKF_op->Rv, Rv);
    Matrix_vCopy_f32(&UKF_op->Rn, Rn);
}

bool UKF_bCalculateSigmaPoint(UKF_t *UKF_op) {
    /* Xs(k-1) = [x(k-1) ... x(k-1)]            ; Xs(k-1) = NxN
     * GPsq = gamma * sqrt(P(k-1))
     * XSigma(k-1) = [x(k-1) Xs(k-1)+GPsq Xs(k-1)-GPsq]                     ...{UKF_4}
     */
    /* Use Cholesky Decomposition to compute sqrt(P) */
    matrix_f32_t _temp;
    Matrix_vinit_f32(&_temp);
    Matrix_vCholeskyDec_f32(&UKF_op->P, &UKF_op->P_Chol);
    if (!Matrix_bMatrixIsValid_f32(&UKF_op->P_Chol)) {
        /* System Fail */
        return false;
    }
    Matrix_vscale_f32(&UKF_op->P_Chol, UKF_op->Gamma);

    /* Xs(k-1) = [x(k-1) ... x(k-1)]            ; Xs(k-1) = NxN */
    matrix_f32_t Y_;
    Matrix_nodata_creat_f32(&Y_, SS_X_LEN, SS_X_LEN, InitMatWithZero);
    for (int16_t _i = 0; _i < SS_X_LEN; _i++) {
        Matrix_vInsertVector_f32(&Y_, &UKF_op->X_Est, _i, &Y_);
    }

    Matrix_vSetToZero_f32(&UKF_op->X_Sigma);
    /* XSigma(k-1) = [x(k-1) 0 0] */
    Matrix_vInsertVector_f32(&UKF_op->X_Sigma, &UKF_op->X_Est, 0, &UKF_op->X_Sigma);
    /* XSigma(k-1) = [x(k-1) Xs(k-1)+GPsq  0] */
    Matrix_vadd_f32(&Y_, &UKF_op->P_Chol, &_temp);
    Matrix_vInsertAllSubMatrix_f32(&UKF_op->X_Sigma, &_temp, 0, 1, &UKF_op->X_Sigma);
    /* XSigma(k-1) = [x(k-1) Xs(k-1)+GPsq Xs(k-1)-GPsq] */
    Matrix_vsub_f32(&Y_, &UKF_op->P_Chol, &_temp);
    Matrix_vInsertAllSubMatrix_f32(&UKF_op->X_Sigma, &_temp, 0, (1 + SS_X_LEN), &UKF_op->X_Sigma);
    Matrix_vSetMatrixInvalid_f32(&Y_);
    Matrix_vSetMatrixInvalid_f32(&_temp);

    return true;
}

bool
UKF_bUnscentedTransform(UKF_t *UKF_op, matrix_f32_t *Out, matrix_f32_t *OutSigma, matrix_f32_t *P, matrix_f32_t *DSig,
                        bool (*_vFuncNonLinear)(matrix_f32_t *xOut, matrix_f32_t *xInp, matrix_f32_t *U,
                                                AHRS_t *AHRS_op),
                        matrix_f32_t *InpSigma, matrix_f32_t *InpVector,
                        matrix_f32_t *_CovNoise, AHRS_t *AHRS_op) {
    matrix_f32_t _temp_T;
    matrix_f32_t _temp_M;
    Matrix_vinit_f32(&_temp_T);
    Matrix_vinit_f32(&_temp_M);
    /* XSigma(k) = f(XSigma(k-1), u(k-1))                                  ...{UKF_5a}  */
    /* x(k|k-1) = sum(Wm(i) * XSigma(k)(i))    ; i = 1 ... (2N+1)          ...{UKF_6a}  */
    Matrix_vSetToZero_f32(Out);
    for (int16_t _j = 0; _j < InpSigma->arm_matrix.numCols; _j++) {
        /* Transform the column submatrix of sigma-points input matrix (InpSigma) */
        matrix_f32_t _AuxSigma1;
        matrix_f32_t _AuxSigma2;
        Matrix_nodata_creat_f32(&_AuxSigma1, InpSigma->arm_matrix.numRows, 1, InitMatWithZero);
        Matrix_nodata_creat_f32(&_AuxSigma2, OutSigma->arm_matrix.numRows, 1, InitMatWithZero);
        for (int16_t _i = 0; _i < InpSigma->arm_matrix.numRows; _i++) {
            _AuxSigma1.p2Data[_i][0] = InpSigma->p2Data[_i][_j];
        }
        if (!_vFuncNonLinear(&_AuxSigma2, &_AuxSigma1, InpVector, AHRS_op)) {
            Matrix_vSetMatrixInvalid_f32(&_AuxSigma1);
            Matrix_vSetMatrixInvalid_f32(&_AuxSigma2);
            /* Somehow the transformation function is failed, propagate the error */
            return false;
        }

        /* Combine the transformed vector to construct sigma-points output matrix (OutSigma) */
        Matrix_vInsertVector_f32(OutSigma, &_AuxSigma2, _j, OutSigma);

        /* Calculate x(k|k-1) as weighted mean of OutSigma */
        Matrix_vscale_f32(&_AuxSigma2, UKF_op->Wm.p2Data[0][_j]);
        Matrix_vadd_f32(Out, &_AuxSigma2, Out);
        Matrix_vSetMatrixInvalid_f32(&_AuxSigma1);
        Matrix_vSetMatrixInvalid_f32(&_AuxSigma2);
    }

    /* DX = XSigma(k)(i) - Xs(k)   ; Xs(k) = [x(k|k-1) ... x(k|k-1)]
     *                             ; Xs(k) = Nx(2N+1)                      ...{UKF_7a}  */
    matrix_f32_t _AuxSigma1;
    Matrix_nodata_creat_f32(&_AuxSigma1, OutSigma->arm_matrix.numRows, OutSigma->arm_matrix.numCols, InitMatWithZero);
    for (int16_t _j = 0; _j < OutSigma->arm_matrix.numCols; _j++) {
        Matrix_vInsertVector_f32(&_AuxSigma1, Out, _j, &_AuxSigma1);
    }
    Matrix_vsub_f32(OutSigma, &_AuxSigma1, DSig);
    /* P(k|k-1) = sum(Wc(i)*DX*DX') + Rv       ; i = 1 ... (2N+1)          ...{UKF_8a}  */
    Matrix_vCopy_f32(DSig, &_AuxSigma1);
    for (int16_t _i = 0; _i < DSig->arm_matrix.numRows; _i++) {
        for (int16_t _j = 0; _j < DSig->arm_matrix.numCols; _j++) {
            _AuxSigma1.p2Data[_i][_j] *= UKF_op->Wc.p2Data[0][_j];
        }
    }
    Matrix_vTranspose_nsame_f32(DSig, &_temp_T);
    Matrix_vmult_nsame_f32(&_AuxSigma1, &_temp_T, &_temp_M);
    Matrix_vadd_f32(&_temp_M, _CovNoise, P);
    Matrix_vSetMatrixInvalid_f32(&_AuxSigma1);
    Matrix_vSetMatrixInvalid_f32(&_temp_T);
    Matrix_vSetMatrixInvalid_f32(&_temp_M);

    return true;
}

bool AHRS_bUpdateNonlinearX(matrix_f32_t *X_Next, matrix_f32_t *X_matrix, matrix_f32_t *U_matrix, AHRS_t *AHRS_op) {
    //此处更改传感器投影方向，根据陀螺仪对对应符号取负，如下英文注释参考MPU9250,https://github.com/pronenewbits/Arduino_AHRS_System板子实物图，此处相对其x不动在The quaternion update function部分对q,r做取负修正
    /* Insert the nonlinear update transformation here
 *          x(k+1) = f[x(k), u(k)]
 *
 * The quaternion update function:
 *  q0_dot = 1/2. * (  0   - p*q1 - q*q2 - r*q3)
 *  q1_dot = 1/2. * ( p*q0 +   0  + r*q2 - q*q3)
 *  q2_dot = 1/2. * ( q*q0 - r*q1 +  0   + p*q3)
 *  q3_dot = 1/2. * ( r*q0 + q*q1 - p*q2 +  0  )
 *
 * Euler method for integration:
 *  q0 = q0 + q0_dot * dT;
 *  q1 = q1 + q1_dot * dT;
 *  q2 = q2 + q2_dot * dT;
 *  q3 = q3 + q3_dot * dT;
 */
    float32_t q0, q1, q2, q3;
    float32_t p, q, r;

    q0 = X_matrix->p2Data[0][0];
    q1 = X_matrix->p2Data[1][0];
    q2 = X_matrix->p2Data[2][0];
    q3 = X_matrix->p2Data[3][0];

    p = U_matrix->p2Data[0][0];
    q = U_matrix->p2Data[1][0];
    r = U_matrix->p2Data[2][0];

    X_Next->p2Data[0][0] = (0.5f * (+0.00f + p * q1 + q * q2 + r * q3)) * SS_DT + q0;
    X_Next->p2Data[1][0] = (0.5f * (-p * q0 + 0.00f - r * q2 + q * q3)) * SS_DT + q1;
    X_Next->p2Data[2][0] = (0.5f * (-q * q0 - r * q1 + 0.00f - p * q3)) * SS_DT + q2;
    X_Next->p2Data[3][0] = (0.5f * (-r * q0 - q * q1 + p * q2 + 0.00f)) * SS_DT + q3;


    /* ======= Additional ad-hoc quaternion normalization to make sure the quaternion is a unit vector (i.e. ||q|| = 1) ======= */
    if (!Matrix_bNormVector_f32(X_Next)) {
        /* System error, return false so we can reset the UKF */
        return false;
    }

    return true;
}

bool AHRS_bUpdateNonlinearY(matrix_f32_t *Y_matrix, matrix_f32_t *X_matrix, matrix_f32_t *U_matrix, AHRS_t *AHRS_op) {
    //此处更改传感器投影方向IMU_ACC_Z0，原MPU9250
    /* Insert the nonlinear measurement transformation here
     *          y(k)   = h[x(k), u(k)]
     *
     * The measurement output is the gravitational and magnetic projection to the body
     *     DCM     = [(+(q0**2)+(q1**2)-(q2**2)-(q3**2)),                        2*(q1*q2+q0*q3),                        2*(q1*q3-q0*q2)]
     *               [                   2*(q1*q2-q0*q3),     (+(q0**2)-(q1**2)+(q2**2)-(q3**2)),                        2*(q2*q3+q0*q1)]
     *               [                   2*(q1*q3+q0*q2),                        2*(q2*q3-q0*q1),     (+(q0**2)-(q1**2)-(q2**2)+(q3**2))]
     *
     *  G_proj_sens = DCM * [0 0 1]             --> Gravitational projection to the accelerometer sensor
     *  M_proj_sens = DCM * [Mx My Mz]          --> (Earth) magnetic projection to the magnetometer sensor //依赖初始四元数
     */
    float32_t q0, q1, q2, q3;
    float32_t q0_2, q1_2, q2_2, q3_2;

    q0 = X_matrix->p2Data[0][0];
    q1 = X_matrix->p2Data[1][0];
    q2 = X_matrix->p2Data[2][0];
    q3 = X_matrix->p2Data[3][0];

    q0_2 = q0 * q0;
    q1_2 = q1 * q1;
    q2_2 = q2 * q2;
    q3_2 = q3 * q3;

    Y_matrix->p2Data[0][0] = (2 * q1 * q3 - 2 * q0 * q2) * IMU_ACC_Z0;

    Y_matrix->p2Data[1][0] = (2 * q2 * q3 + 2 * q0 * q1) * IMU_ACC_Z0;

    Y_matrix->p2Data[2][0] = (+(q0_2) - (q1_2) - (q2_2) + (q3_2)) * IMU_ACC_Z0;

    Y_matrix->p2Data[3][0] = (+(q0_2) + (q1_2) - (q2_2) - (q3_2)) * AHRS_op->IMU_MAG_B0.p2Data[0][0]
                             + (2 * (q1 * q2 + q0 * q3)) * AHRS_op->IMU_MAG_B0.p2Data[1][0]
                             + (2 * (q1 * q3 - q0 * q2)) * AHRS_op->IMU_MAG_B0.p2Data[2][0];

    Y_matrix->p2Data[4][0] = (2 * (q1 * q2 - q0 * q3)) * AHRS_op->IMU_MAG_B0.p2Data[0][0]
                             + (+(q0_2) - (q1_2) + (q2_2) - (q3_2)) * AHRS_op->IMU_MAG_B0.p2Data[1][0]
                             + (2 * (q2 * q3 + q0 * q1)) * AHRS_op->IMU_MAG_B0.p2Data[2][0];

    Y_matrix->p2Data[5][0] = (2 * (q1 * q3 + q0 * q2)) * AHRS_op->IMU_MAG_B0.p2Data[0][0]
                             + (2 * (q2 * q3 - q0 * q1)) * AHRS_op->IMU_MAG_B0.p2Data[1][0]
                             + (+(q0_2) - (q1_2) - (q2_2) + (q3_2)) * AHRS_op->IMU_MAG_B0.p2Data[2][0];
    return true;
}

void NEWAHRS_init(AHRS_t *AHRS_op) {
    Matrix_data_creat_f32(&AHRS_op->IMU_MAG_B0, 3, 1, IMU_MAG_B0_data, InitMatWithZero);
    Matrix_data_creat_f32(&AHRS_op->HARD_IRON_BIAS, 3, 1, HARD_IRON_BIAS_data, InitMatWithZero);
    Matrix_data_creat_f32(&quaternionData, SS_X_LEN, 1, INS_quat, InitMatWithZero);

    Matrix_nodata_creat_f32(&RLS_theta, 4, 1, InitMatWithZero);
    Matrix_nodata_creat_f32(&RLS_P, 4, 4, InitMatWithZero);
    Matrix_nodata_creat_f32(&RLS_in, 4, 1, InitMatWithZero);
    Matrix_nodata_creat_f32(&RLS_out, 1, 1, InitMatWithZero);
    Matrix_nodata_creat_f32(&RLS_gain, 4, 1, InitMatWithZero);

    Matrix_nodata_creat_f32(&Y, SS_Z_LEN, 1, InitMatWithZero);
    Matrix_nodata_creat_f32(&U, SS_U_LEN, 1, InitMatWithZero);

    Matrix_data_creat_f32(&UKF_PINIT, SS_X_LEN, SS_X_LEN, UKF_PINIT_data, NoInitMatZero);

    Matrix_data_creat_f32(&UKF_Rv, SS_X_LEN, SS_X_LEN, UKF_Rv_data, NoInitMatZero);

    Matrix_data_creat_f32(&UKF_Rn, SS_Z_LEN, SS_Z_LEN, UKF_Rn_data, NoInitMatZero);

    /* UKF initialization ----------------------------------------- */
    /* x(k=0) = [1 0 0 0]' */
//    Matrix_vSetToZero_f32(&quaternionData);

    UKF_init(&UKF_IMU, &quaternionData, &UKF_PINIT, &UKF_Rv, &UKF_Rn, AHRS_bUpdateNonlinearX, AHRS_bUpdateNonlinearY);
    /* RLS initialization ----------------------------------------- */
    Matrix_vSetToZero_f32(&RLS_theta);
    Matrix_vSetIdentity_f32(&RLS_P);
    Matrix_vscale_f32(&RLS_P, 1000.0f);
}

void AHRS_quaternion_init(AHRS_t *AHRS_op) {
    //quaterniondata [w x y z]
//    ist8310_read_over(mag_dma_rx_buf, ist8310_real_data.mag);
//    BMI088_read(bmi088_real_data.gyro, bmi088_real_data.accel, &bmi088_real_data.temp);
//    imu_mag_rotate(INS_gyro, INS_accel, INS_mag, &bmi088_real_data, &ist8310_real_data);
    float32_t Ax = INS_accel_cali[0];
    float32_t Ay = INS_accel_cali[1];
    float32_t Az = INS_accel_cali[2];
//    float32_t Bx = ist8310_real_data.mag[0] - IMU.HARD_IRON_BIAS.p2Data[0][0];
//    float32_t By = ist8310_real_data.mag[1] - IMU.HARD_IRON_BIAS.p2Data[1][0];
//    float32_t Bz = ist8310_real_data.mag[2] - IMU.HARD_IRON_BIAS.p2Data[2][0];
    float32_t Bx = INS_mag_cali[0];
    float32_t By = INS_mag_cali[1];
    float32_t Bz = INS_mag_cali[2];

    /* Normalizing the acceleration vector & projecting the gravitational vector (gravity is negative acceleration) */
    float32_t _normG = sqrtf((Ax * Ax) + (Ay * Ay) + (Az * Az));
    Ax = Ax / _normG;
    Ay = Ay / _normG;
    Az = Az / _normG;

    /* Normalizing the magnetic vector */
    _normG = sqrtf((Bx * Bx) + (By * By) + (Bz * Bz));
    Bx = Bx / _normG;
    By = By / _normG;
    Bz = Bz / _normG;

    /* Projecting the magnetic vector into plane orthogonal to the gravitational vector */
    float32_t pitch = asinf(Ax);
    float32_t roll = atan2f(Ay, Az);
    float32_t m_tilt_x = Bx * cosf(pitch) + By * sinf(roll) * sinf(pitch) + Bz * cosf(roll) * sinf(pitch);
    float32_t m_tilt_y = By * cosf(roll) - Bz * sinf(roll);
//    float32_t m_tilt_x = Bx * cosf(roll) + Bz * sinf(roll);
//    float32_t m_tilt_y = By * cosf(pitch) + Bx * sinf(roll) * sinf(pitch) - Bz * cosf(roll) * sinf(pitch);
    /* float32_t m_tilt_z = -Bx*sin(pitch)             + By*sin(roll)*cos(pitch)   + Bz*cos(roll)*cos(pitch); */

    float32_t yaw = atan2f(m_tilt_y, m_tilt_x);
//    float32_t yaw = 0;

    //ZYX(yaw-pitch-roll)
    INS_quat[0] =
            cosf(yaw / 2.0f) * cosf(pitch / 2.0f) * cosf(roll / 2.0f) + sinf(yaw / 2.0f) * sinf(pitch / 2.0f) *
                                                                        sinf(roll / 2.0f);
    INS_quat[1] =
            cosf(yaw / 2.0f) * cosf(pitch / 2.0f) * sinf(roll / 2.0f) - sinf(yaw / 2.0f) * sinf(pitch / 2.0f) *
                                                                        cosf(roll / 2.0f);
    INS_quat[2] =
            cosf(yaw / 2.0f) * sinf(pitch / 2.0f) * cosf(roll / 2.0f) + sinf(yaw / 2.0f) * cosf(pitch / 2.0f) *
                                                                        sinf(roll / 2.0f);
    INS_quat[3] =
            sinf(yaw / 2.0f) * cosf(pitch / 2.0f) * cosf(roll / 2.0f) - cosf(yaw / 2.0f) * sinf(pitch / 2.0f) *
                                                                        sinf(roll / 2.0f);
//    SEGGER_RTT_printf(0,"%f\r\n",INS_quat[0]);
//    SEGGER_RTT_printf(0,"%f\r\n",INS_quat[1]);
//    SEGGER_RTT_printf(0,"%f\r\n",INS_quat[2]);
//    SEGGER_RTT_printf(0,"%f\r\n",INS_quat[3]);
    Matrix_vassignment_f32(&quaternionData, 1, 1, INS_quat[0]);
    Matrix_vassignment_f32(&quaternionData, 2, 1, INS_quat[1]);
    Matrix_vassignment_f32(&quaternionData, 3, 1, INS_quat[2]);
    Matrix_vassignment_f32(&quaternionData, 4, 1, INS_quat[3]);
//    AHRS_op->IMU_MAG_B0.p2Data[0][0] = cosf(yaw);
//    AHRS_op->IMU_MAG_B0.p2Data[1][0] = sinf(yaw);
//    AHRS_op->IMU_MAG_B0.p2Data[2][0] = 0;

}

float32_t AHRS_get_instant_pitch(void) {
    float32_t Ax = INS_accel_cali[0];
    float32_t Ay = INS_accel_cali[1];
    float32_t Az = INS_accel_cali[2];
    float32_t _normG = sqrtf((Ax * Ax) + (Ay * Ay) + (Az * Az));
    Ay = Ay / _normG;
    return -asinf(Ay);
}

float32_t AHRS_get_instant_roll(void) {
    float32_t Ax = INS_accel_cali[0];
    float32_t Ay = INS_accel_cali[1];
    float32_t Az = INS_accel_cali[2];
    float32_t _normG = sqrtf((Ax * Ax) + (Ay * Ay) + (Az * Az));
    Ax = Ax / _normG;
    Az = Az / _normG;
    return atan2f(Ax, Az);
}

float32_t AHRS_get_instant_yaw(void) {
    float32_t Ax = INS_mag_cali[0];
    float32_t Ay = INS_mag_cali[1];
    float32_t Az = INS_mag_cali[2];
    float32_t _normG = sqrtf((Ax * Ax) + (Ay * Ay) + (Az * Az));
    Ax = Ax / _normG;
    Ay = Ay / _normG;
    Az = Az / _normG;
    return atan2f(Ax*cosf(AHRS_get_instant_pitch())+Ay*sinf(AHRS_get_instant_pitch())*sinf(AHRS_get_instant_roll()+Az*cosf(AHRS_get_instant_roll())*sinf(AHRS_get_instant_pitch())), Ay*cosf(AHRS_get_instant_roll())-Az*sinf(AHRS_get_instant_roll())) + M_PI;
}