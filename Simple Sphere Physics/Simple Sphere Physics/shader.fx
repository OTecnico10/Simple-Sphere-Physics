cbuffer ConstantBuffer : register( b0 )
{
	matrix World;
	matrix View;
	matrix Projection;
	matrix WorldInvTrans;
	float4 color;
	float3 LightDir;
	float3 EyePos;
	float EyeDir;
}


struct VS_INPUT
{
    float4 Pos : POSITION;
	float3 Norm : NORMAL;
};

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float3 Norm : NORMAL;
	float3 LightDir : POSITION0;
	float3 EyeVector : POSITION1;
	float4 color :	COLOR;
};

PS_INPUT VS( VS_INPUT input )
{
    PS_INPUT output = (PS_INPUT)0;
    output.Pos = mul( input.Pos, World );
	output.EyeVector = normalize(EyePos - output.Pos);
    output.Pos = mul( output.Pos, View );
    output.Pos = mul( output.Pos, Projection );
	output.Norm = mul( input.Norm, WorldInvTrans );
	output.LightDir = -LightDir;
	output.color = color;

    return output;
}

float4 PS( PS_INPUT input) : SV_Target
{
	input.Norm = normalize( input.Norm );

	float specTerm = 0.0f;

	float3 ReflVector = normalize( reflect( input.LightDir, input.Norm ) );

	[flatten]
	if ( dot( ReflVector, normalize(-EyeDir) ) >= 0 )
	{
		specTerm = pow(  dot( ReflVector, normalize(-EyeDir) ) , 50 );
	}
	float diffuseTerm = saturate( dot( input.LightDir, input.Norm ) );
	float4 ambient = float4( 0.25f, 0.25f, 0.25f, 1.0f );
	float4 lightColor = float4( 1.0f, 1.0f, 1.0f, 1.0f );
    return (ambient + (diffuseTerm + specTerm)) * lightColor * input.color;
}