// DSMusicBuffer.cpp
// KlayGE DirectSound音乐缓冲区类 实现文件
// Ver 2.0.0
// 版权所有(C) 龚敏敏, 2003
// Homepage: http://www.enginedev.com
//
// 2.0.0
// 初次建立 (2003.10.4)
//
// 修改记录
/////////////////////////////////////////////////////////////////////////////////

#include <KlayGE/KlayGE.hpp>
#include <KlayGE/ThrowErr.hpp>
#include <KlayGE/SharedPtr.hpp>
#include <KlayGE/Memory.hpp>
#include <KlayGE/Engine.hpp>
#include <KlayGE/AudioFactory.hpp>
#include <KlayGE/AudioDataSource.hpp>

#include <cassert>

#include <KlayGE/DSound/DSAudio.hpp>

namespace KlayGE
{
	// 构造函数。建立一个可以用于流式播放的缓冲区
	/////////////////////////////////////////////////////////////////////////////////
	DSMusicBuffer::DSMusicBuffer(const AudioDataSourcePtr& dataSource, U32 bufferSeconds, float volume)
					: MusicBuffer(dataSource),
						writePos_(0)
	{
		WAVEFORMATEX wfx(WaveFormatEx(dataSource));
		notifySize_		= wfx.nAvgBytesPerSec / PreSecond;
		notifyCount_	= bufferSeconds * PreSecond;

		bool mono(1 == wfx.nChannels);

		const COMPtr<IDirectSound>& dsound(static_cast<DSAudioEngine&>(Engine::AudioFactoryInstance().AudioEngineInstance()).DSound());

		// 建立 DirectSound 缓冲区，要尽量减少使用建立标志，
		// 因为使用太多不必要的标志会影响硬件加速性能
		DSBUFFERDESC dsbd;
		Engine::MemoryInstance().Zero(&dsbd, sizeof(dsbd));
		dsbd.dwSize				= sizeof(dsbd);
		dsbd.dwFlags			= DSBCAPS_CTRLPOSITIONNOTIFY;
		if (mono)
		{
			dsbd.dwFlags |= DSBCAPS_CTRL3D | DSBCAPS_MUTE3DATMAXDISTANCE;
			dsbd.guid3DAlgorithm	= GUID_NULL;
		}
		dsbd.dwBufferBytes		= notifySize_ * notifyCount_;
		dsbd.lpwfxFormat		= &wfx;

		// DirectSound只能播放PCM数据。其他格式可能不能工作。
		IDirectSoundBuffer* buffer;
		TIF(dsound->CreateSoundBuffer(&dsbd, &buffer, NULL));
		buffer_ = COMPtr<IDirectSoundBuffer>(buffer);

		if (mono)
		{
			buffer_.QueryInterface<IID_IDirectSound3DBuffer>(ds3DBuffer_);
		}

		notifyEvent_ = ::CreateEventW(NULL, FALSE, FALSE, NULL);

		COMPtr<IDirectSoundNotify> dsNotify;
		TIF(buffer_.QueryInterface<IID_IDirectSoundNotify>(dsNotify));

		std::vector<DSBPOSITIONNOTIFY> posNotify(notifyCount_);
		for (std::vector<DSBPOSITIONNOTIFY>::iterator iter = posNotify.begin(); iter != posNotify.end(); ++ iter)
		{
			iter->dwOffset     = (notifySize_ * std::distance(posNotify.begin(), iter)) + notifySize_ - 1;
			iter->hEventNotify = notifyEvent_;
		}

		// Tell DirectSound when to notify us. The notification will come in the from 
		// of signaled events that are handled in WinMain()
		TIF(dsNotify->SetNotificationPositions(notifyCount_, &posNotify[0]));


		this->Position(Vector3::Zero());
		this->Velocity(Vector3::Zero());
		this->Direction(Vector3::Zero());

		this->Volume(volume);

		this->Reset();
	}

	// 析构函数
	/////////////////////////////////////////////////////////////////////////////////
	DSMusicBuffer::~DSMusicBuffer()
	{
		this->Stop();
		::CloseHandle(notifyEvent_);
	}

	// 更新缓冲区
	/////////////////////////////////////////////////////////////////////////////////
	void DSMusicBuffer::LoopUpdateBuffer()
	{
		bool finished(false);

		for (; !finished;)
		{
			if (WAIT_OBJECT_0 == ::WaitForMultipleObjects(1, &notifyEvent_, FALSE, INFINITE))
			{
				// 锁定缓冲区
				void* lockedBuffer;			// 指向缓冲区锁定的内存的指针
				U32   lockedBufferSize;		// 锁定的内存大小
				TIF(buffer_->Lock(notifySize_ * writePos_, notifySize_,
					&lockedBuffer, &lockedBufferSize, NULL, NULL, 0));

				std::vector<U8, alloc<U8> > data(notifySize_);
				data.resize(dataSource_->Read(&data[0], notifySize_));

				if (data.size() > 0)
				{
					Engine::MemoryInstance().Cpy(lockedBuffer, &data[0], data.size());

					Engine::MemoryInstance().Set(static_cast<U8*>(lockedBuffer) + data.size(), 
						0, lockedBufferSize - data.size());
				}
				else
				{
					Engine::MemoryInstance().Set(lockedBuffer, 0, lockedBufferSize);
					finished = true;
					this->Stop();
				}

				// 缓冲区解锁
				buffer_->Unlock(lockedBuffer, lockedBufferSize, NULL, 0);

				// 形成环形缓冲区
				++ writePos_;
				writePos_ %= notifyCount_;
			}
		}
	}

	// 缓冲区复位以便于从头播放
	/////////////////////////////////////////////////////////////////////////////////
	void DSMusicBuffer::DoReset()
	{
		this->writePos_	= 0;

		dataSource_->Reset();

		// 锁定缓冲区
		void* lockedBuffer;			// 指向缓冲区锁定的内存的指针
		U32   lockedBufferSize;		// 锁定的内存大小
		TIF(buffer_->Lock(0, notifySize_ * notifyCount_,
			&lockedBuffer, &lockedBufferSize, NULL, NULL, 0));

		std::vector<U8, alloc<U8> > data(notifySize_ * notifyCount_);
		data.resize(dataSource_->Read(&data[0], notifySize_ * notifyCount_));

		if (data.size() > 0)
		{
			// 如果数据源比缓冲区小，则用音频数据填充缓冲区
			Engine::MemoryInstance().Cpy(lockedBuffer, &data[0], data.size());

			// 剩下的区域用空白填充
			Engine::MemoryInstance().Set(static_cast<U8*>(lockedBuffer) + data.size(), 
				0, lockedBufferSize - data.size());
		}
		else
		{
			// 如果音频数据空白，用静音填充
			Engine::MemoryInstance().Set(lockedBuffer, 0, lockedBufferSize);
		}

		// 缓冲区解锁
		buffer_->Unlock(lockedBuffer, lockedBufferSize, NULL, 0);

		buffer_->SetCurrentPosition(0);
	}

	// 播放音频流
	/////////////////////////////////////////////////////////////////////////////////
	void DSMusicBuffer::DoPlay(bool /*loop*/)
	{
		buffer_->Play(0, 0, DSBPLAY_LOOPING);
	}

	// 停止播放音频流
	////////////////////////////////////////////////////////////////////////////////
	void DSMusicBuffer::DoStop()
	{
		buffer_->Stop();
	}

	// 检查缓冲区是否在播放
	/////////////////////////////////////////////////////////////////////////////////
	bool DSMusicBuffer::IsPlaying() const
	{
		if (buffer_.Get() != NULL)
		{
			U32 status;
			buffer_->GetStatus(&status);
			return ((status & DSBSTATUS_PLAYING) != 0);
		}

		return false;
	}

	// 设置音量
	/////////////////////////////////////////////////////////////////////////////////
	void DSMusicBuffer::Volume(float vol)
	{
		buffer_->SetVolume(LinearGainToDB(vol));
	}


	// 获取声源位置
	/////////////////////////////////////////////////////////////////////////////////
	Vector3 DSMusicBuffer::Position() const
	{
		Vector3 ret(Vector3::Zero());

		if (ds3DBuffer_.Get() != NULL)
		{
			D3DVECTOR v;
			ds3DBuffer_->GetPosition(&v);
			ret = MakeVector(v.x, v.y, v.z);
		}

		return ret;
	}

	// 设置声源位置
	/////////////////////////////////////////////////////////////////////////////////
	void DSMusicBuffer::Position(const Vector3& v)
	{
		if (ds3DBuffer_.Get() != NULL)
		{
			ds3DBuffer_->SetPosition(v.x(), v.y(), v.z(), DS3D_IMMEDIATE);
		}
	}

	// 获取声源速度
	/////////////////////////////////////////////////////////////////////////////////
	Vector3 DSMusicBuffer::Velocity() const
	{
		Vector3 ret(Vector3::Zero());

		if (ds3DBuffer_.Get() != NULL)
		{
			D3DVECTOR v;
			ds3DBuffer_->GetVelocity(&v);
			ret = MakeVector(v.x, v.y, v.z);
		}

		return ret;
	}

	// 设置声源速度
	/////////////////////////////////////////////////////////////////////////////////
	void DSMusicBuffer::Velocity(const Vector3& v)
	{
		if (ds3DBuffer_.Get() != NULL)
		{
			ds3DBuffer_->SetVelocity(v.x(), v.y(), v.z(), DS3D_IMMEDIATE);
		}
	}

	// 获取声源方向
	/////////////////////////////////////////////////////////////////////////////////
	Vector3 DSMusicBuffer::Direction() const
	{
		Vector3 ret(Vector3::Zero());

		if (ds3DBuffer_.Get() != NULL)
		{
			D3DVECTOR v;
			ds3DBuffer_->GetConeOrientation(&v);
			ret = MakeVector(v.x, v.y, v.z);
		}

		return ret;
	}

	// 设置声源方向
	/////////////////////////////////////////////////////////////////////////////////
	void DSMusicBuffer::Direction(const Vector3& v)
	{
		if (ds3DBuffer_.Get() != NULL)
		{
			ds3DBuffer_->SetConeOrientation(v.x(), v.y(), v.z(), DS3D_IMMEDIATE);
		}
	}
}
