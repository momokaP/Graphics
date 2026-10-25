#define MAX_LIGHTS 3 // 쉐이더에서도 #define 사용 가능
#define NUM_DIR_LIGHTS 1
#define NUM_POINT_LIGHTS 1
#define NUM_SPOT_LIGHTS 1

// 재질
struct Material
{
    float3 ambient;
    float shininess;
    float3 diffuse;
    float dummy1; // 16 bytes 맞춰주기 위해 추가
    float3 specular;
    float dummy2;
    float3 fresnelR0;
    float dummy3;
};

// 조명
struct Light
{
    float3 strength;
    float fallOffStart;
    float3 direction;
    float fallOffEnd;
    float3 position;
    float spotPower;
};

float3 BlinnPhong(float3 lightStrength, float3 lightVec, float3 normal,
                   float3 toEye, Material mat)
{
    float3 halfway = normalize(toEye + lightVec);
    float hdotn = dot(halfway, normal);
    float3 specular = mat.specular * pow(max(hdotn, 0.0f), mat.shininess);

    return mat.ambient + (mat.diffuse + specular) * lightStrength;
}

float3 ComputeDirectionalLight(Light L, Material mat, float3 normal,
                                float3 toEye)
{
    float3 lightVec = -L.direction;

    float ndotl = max(dot(lightVec, normal), 0.0f);
    float3 lightStrength = L.strength * ndotl;

    // Luna DX12 책에서는 Specular 계산에도
    // Lambert's law가 적용된 lightStrength를 사용합니다.
    return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}

float CalcAttenuation(float d, float falloffStart, float falloffEnd)
{
    // Linear falloff
    return saturate((falloffEnd - d) / (falloffEnd - falloffStart));
}

float3 ComputePointLight(Light L, Material mat, float3 pos, float3 normal,
                          float3 toEye)
{
    float3 lightVec = L.position - pos;

    // 쉐이딩할 지점부터 조명까지의 거리 계산
    float d = length(lightVec);

    // 너무 멀면 조명이 적용되지 않음
    if (d > L.fallOffEnd)
    {
        return float3(0.0, 0.0, 0.0);
    }
    else
    {
        lightVec /= d;

        float ndotl = max(dot(lightVec, normal), 0.0f);
        float3 lightStrength = L.strength * ndotl;

        float att = CalcAttenuation(d, L.fallOffStart, L.fallOffEnd);
        lightStrength *= att;

        return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
    }
}

float3 ComputeSpotLight(Light L, Material mat, float3 pos, float3 normal,
                         float3 toEye)
{
    float3 lightVec = L.position - pos;

    // 쉐이딩할 지점부터 조명까지의 거리 계산
    float d = length(lightVec);

    // 너무 멀면 조명이 적용되지 않음
    if (d > L.fallOffEnd)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }
    else
    {
        lightVec /= d;

        float ndotl = max(dot(lightVec, normal), 0.0f);
        float3 lightStrength = L.strength * ndotl;

        float att = CalcAttenuation(d, L.fallOffStart, L.fallOffEnd);
        lightStrength *= att;

        float spotFactor = pow(max(-dot(lightVec, L.direction), 0.0f), L.spotPower);
        lightStrength *= spotFactor;

        return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
    }
    
    // if에 else가 없을 경우 경고 발생
    // warning X4000: use of potentially uninitialized variable
}

cbuffer BasicPixelConstantBuffer : register(b0)
{
    float3 eyeWorld;
    uint useTexture;
    Material material;
    Light light[MAX_LIGHTS];
    float3 rimColor;
    float rimPower;
    float rimStrength;
    uint useSmoothstep;
};

struct PSInput
{
    float4 posProj : SV_POSITION; // Screen position
    float3 posWorld : POSITION; // World position (조명 계산에 사용)
    float3 normalWorld : NORMAL;
    float2 texcoord : TEXCOORD;
    float3 color : COLOR; // Normal lines 쉐이더에서 사용
};

Texture2D g_texture0 : register(t0);
TextureCube g_textureCube : register(t1);
TextureCube g_textureCubeDiffuse : register(t2);
TextureCube g_textureCubeSpecular : register(t3);

SamplerState g_sampler : register(s0);
SamplerState g_linearSampler : register(s1);

float3 ACESFilm(float3 x)   // 근사 ACES 톤매핑
{
    return saturate((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14));
}

// Schlick approximation: Eq. 9.17 in "Real-Time Rendering 4th Ed."
// fresnelR0는 물질의 고유 성질
// Water : (0.02, 0.02, 0.02)
// Glass : (0.08, 0.08, 0.08)
// Plastic : (0.05, 0.05, 0.05)
// Gold: (1.0, 0.71, 0.29)
// Silver: (0.95, 0.93, 0.88)
// Copper: (0.95, 0.64, 0.54)
float3 SchlickFresnel(float3 fresnelR0, float3 normal, float3 toEye)
{
    // 참고 자료들
    // THE SCHLICK FRESNEL APPROXIMATION by Zander Majercik, NVIDIA
    // http://psgraphics.blogspot.com/2020/03/fresnel-equations-schlick-approximation.html
    
    float normalDotView = saturate(dot(normal, toEye));

    float f0 = 1.0f - normalDotView; // 90도이면 f0 = 1, 0도이면 f0 = 0

    // 1.0 보다 작은 값은 여러 번 곱하면 더 작은 값이 됩니다.
    // 0도 -> f0 = 0 -> fresnelR0 반환
    // 90도 -> f0 = 1.0 -> float3(1.0) 반환
    // 0도에 가까운 가장자리는 Specular 색상, 90도에 가까운 안쪽은 고유 색상(fresnelR0)
    return fresnelR0 + (1.0f - fresnelR0) * pow(f0, 5.0);
}

float4 main(PSInput input) : SV_TARGET
{   
    float3 toEye = normalize(eyeWorld - input.posWorld);

    float3 color = float3(0.0, 0.0, 0.0);
    
    int i = 0;
    
    // https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-for
    // https://forum.unity.com/threads/what-are-unroll-and-loop-when-to-use-them.1283096/
    
    [unroll] // warning X3557: loop only executes for 1 iteration(s), forcing loop to unroll
    for (i = 0; i < NUM_DIR_LIGHTS; ++i)
    {
        color += ComputeDirectionalLight(light[i], material, input.normalWorld, toEye);
    }
    
    [unroll]
    for (i = NUM_DIR_LIGHTS; i < NUM_DIR_LIGHTS + NUM_POINT_LIGHTS; ++i)
    {
        color += ComputePointLight(light[i], material, input.posWorld, input.normalWorld, toEye);
    }
    
    [unroll]
    for (i = NUM_DIR_LIGHTS + NUM_POINT_LIGHTS; i < NUM_DIR_LIGHTS + NUM_POINT_LIGHTS + NUM_SPOT_LIGHTS; ++i)
    {
        color += ComputeSpotLight(light[i], material, input.posWorld, input.normalWorld, toEye);
    }
    
    //return float4(color, 1.0);
    float rim = (1.0 - dot(input.normalWorld, toEye));
    
    if (useSmoothstep)
        rim = smoothstep(0.0, 1.0, rim);
    
    rim = pow(abs(rim), rimPower);
    
    color += rim * rimColor * rimStrength;

    // return g_textureCube0.Sample(g_linearSampler, reflect(-toEye, input.normalWorld));
    
    {
        float3 N = input.normalWorld;
        float3 R = reflect(-toEye, N);

        // Blinn-Phong 광택 → 거칠기 근사 (shininess 1 → 거침, 256 → 매끈)
        float roughness = sqrt(2.0 / (material.shininess + 2.0));

        uint w, h, mipCount;
        g_textureCubeSpecular.GetDimensions(0, w, h, mipCount);
        float lod = roughness * (mipCount - 1);

        float3 irradiance = g_textureCubeDiffuse.Sample(g_linearSampler, N).rgb;
        float3 prefiltered = g_textureCubeSpecular.SampleLevel(g_linearSampler, R, lod).rgb;

        float3 albedo = useTexture ? g_texture0.Sample(g_linearSampler, input.texcoord).rgb
                               : float3(1.0, 1.0, 1.0);
        float3 F = SchlickFresnel(material.fresnelR0, N, toEye);

        float3 diffuse = irradiance * material.diffuse * albedo;
        float3 specular = prefiltered * material.specular * F;

        return float4(diffuse + specular, 1.0); // 모든 항이 환경 밝기에 선형 → 노출이 그대로 반영됨
    }
    
    {
        //float3 diffuse = g_textureCubeDiffuse.Sample(g_sampler, input.normalWorld).rgb;
        //float3 specular = g_textureCube.Sample(g_sampler, reflect(-toEye, input.normalWorld)).rgb;

        //float avg = (specular.x + specular.y + specular.z) / 3.0;
        //specular *= pow(saturate(avg), material.shininess);

        //diffuse *= material.diffuse;
        //specular *= material.specular;

        //float3 f = SchlickFresnel(material.fresnelR0, input.normalWorld, toEye);
        //specular *= f;

        //if (useTexture)
        //    diffuse *= g_texture0.Sample(g_sampler, input.texcoord).rgb;

        //return float4(diffuse + specular, 1.0);
    }
    
    {
        //float3 hdr = g_textureCube.Sample(g_linearSampler, reflect(-toEye, input.normalWorld));

        //const float exposure = 1.0; // 밝기 조절 (1~3 정도로 바꿔 보기)
        //float3 color3 = ACESFilm(hdr * exposure); // HDR → 0~1
        //color3 = pow(color3, 1.0 / 2.2); // 선형 → 감마 (모니터용)

        //return float4(color3, 1.0);
    }

    
    //return useTexture ? float4(color, 1.0) * g_texture0.Sample(g_sampler, input.texcoord) : float4(color, 1.0);
}