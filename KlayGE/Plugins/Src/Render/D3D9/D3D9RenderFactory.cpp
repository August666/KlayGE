// D3D9RenderFactory.cpp
// KlayGE D3D9渲染引擎抽象工厂 实现文件
// Ver 2.0.3
// 版权所有(C) 龚敏敏, 2003-2004
// Homepage: http://klayge.sourceforge.net
//
// 2.0.3
// 改为template实现 (2004.3.4)
//
// 2.0.0
// 初次建立 (2003.8.30)
//
// 修改记录
/////////////////////////////////////////////////////////////////////////////////

#include <KlayGE/KlayGE.hpp>
#include <KlayGE/D3D9/D3D9RenderEngine.hpp>
#include <KlayGE/D3D9/D3D9Texture.hpp>
#include <KlayGE/D3D9/D3D9RenderTexture.hpp>
#include <KlayGE/D3D9/D3D9RenderEffect.hpp>
#include <KlayGE/D3D9/D3D9Font.hpp>
#include <KlayGE/D3D9/D3D9VertexBuffer.hpp>

#include <KlayGE/D3D9/D3D9RenderFactory.hpp>

namespace KlayGE
{
	class D3D9RenderFactory : public ConcreteRenderFactory<D3D9RenderEngine, D3D9Texture, D3D9RenderTexture,
			D3D9Font, D3D9RenderEffect>
	{
	public:
		D3D9RenderFactory()
			: ConcreteRenderFactory<D3D9RenderEngine, D3D9Texture, D3D9RenderTexture,
				D3D9Font, D3D9RenderEffect>(L"Direct3D9 Render Factory")
			{ }
			
		VertexStreamPtr MakeVertexStream(VertexStreamType type,
			U8 elementSize, U8 elementNum, bool staticStream = false)
		{
			VertexStreamPtr stream;

			switch (type)
			{
			case VST_Positions:
				if (staticStream)
				{
					stream = VertexStreamPtr(new D3D9StaticVertexStream(type, sizeof(float), 3));
				}
				else
				{
					stream = VertexStreamPtr(new DynamicVertexStream<float>(type, 3));
				}
				break;

			case VST_Normals:
				if (staticStream)
				{
					stream = VertexStreamPtr(new D3D9StaticVertexStream(type, sizeof(float), 3));
				}
				else
				{
					stream = VertexStreamPtr(new DynamicVertexStream<float>(type, 3));
				}
				break;

			case VST_Diffuses:
				if (staticStream)
				{
					stream = VertexStreamPtr(new D3D9StaticVertexStream(type, sizeof(D3DCOLOR), 1));
				}
				else
				{
					stream = VertexStreamPtr(new DynamicVertexStream<D3DCOLOR>(type, 1));
				}
				break;

			case VST_Speculars:
				if (staticStream)
				{
					stream = VertexStreamPtr(new D3D9StaticVertexStream(type, sizeof(D3DCOLOR), 1));
				}
				else
				{
					stream = VertexStreamPtr(new DynamicVertexStream<D3DCOLOR>(type, 1));
				}
				break;

			case VST_BlendWeights:
				if (staticStream)
				{
					stream = VertexStreamPtr(new D3D9StaticVertexStream(type, sizeof(float), 4));
				}
				else
				{
					stream = VertexStreamPtr(new DynamicVertexStream<float>(type, 4));
				}
				break;

			case VST_BlendIndices:
				if (staticStream)
				{
					stream = VertexStreamPtr(new D3D9StaticVertexStream(type, sizeof(U8), 4));
				}
				else
				{
					stream = VertexStreamPtr(new DynamicVertexStream<U8>(type, 4));
				}
				break;

			default:
				if (staticStream)
				{
					stream = VertexStreamPtr(new D3D9StaticVertexStream(type, sizeof(float), elementNum));
				}
				else
				{
					stream = VertexStreamPtr(new DynamicVertexStream<float>(type, elementNum));
				}
				break;
			}

			return stream;
		}

		IndexStreamPtr MakeIndexStream(bool staticStream = false)
		{
			if (staticStream)
			{
				return IndexStreamPtr(new D3D9StaticIndexStream);
			}
			else
			{
				return IndexStreamPtr(new DynamicIndexStream);
			}
		}
	};

	RenderFactory& D3D9RenderFactoryInstance()
	{
		static D3D9RenderFactory renderFactory;
		return renderFactory;
	}
}
