/*
 * 这个驱动展示了ASoC（ALSA System on Chip）的基本工作原理：
 * 1. 声卡 = CPU控制器 + 编解码器
 * 2. DAI链路定义数据流方向
 * 3. 设备树告诉驱动硬件连接关系
 *
 * 设计原则：
 * - 注释详尽，代码极简
 * - 每个函数只做一件事
 * - 高内聚（相关功能在一起），低耦合（模块间独立）
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <sound/soc.h>
#include <sound/wm8960.h>

// =============================================================================
// 模块1：私有数据结构
// 作用：保存驱动需要的私有信息
// =============================================================================

/*
 * 私有数据结构体
 * 每个声卡设备都有自己的私有数据，用来保存运行时状态
 */
struct wm8960_card_data {
	struct clk *mclk;        // 主时钟（Master Clock）
	bool mclk_enabled;       // 时钟是否已启用
};

// =============================================================================
// 模块2：音频参数配置
// 作用：当音频播放开始时，配置编解码器的时钟
// =============================================================================

/*
 * 硬件参数设置函数
 * 这个函数在每次音频播放/录音开始时被调用
 * 作用：告诉WM8960编解码器使用什么时钟频率
 */
static int wm8960_hw_params(struct snd_pcm_substream *substream,
			   struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;  // 运行时数据
	struct snd_soc_card *card = rtd->card;                     // 声卡结构体
	struct wm8960_card_data *priv = snd_soc_card_get_drvdata(card); // 私有数据
	struct snd_soc_dai *codec_dai = snd_soc_rtd_to_codec(rtd, 0);   // 编解码器DAI

	// 如果有MCLK时钟，就告诉编解码器使用它
	if (priv->mclk) {
		unsigned long mclk_rate = clk_get_rate(priv->mclk);   // 获取时钟频率
		// 设置编解码器的系统时钟
		snd_soc_dai_set_sysclk(codec_dai, 0, mclk_rate, SND_SOC_CLOCK_IN);
	}

	return 0;
}

/*
 * 操作函数集合
 * 告诉ASoC框架我们支持哪些操作
 */
static struct snd_soc_ops wm8960_audio_ops = {
	.hw_params = wm8960_hw_params,  // 硬件参数设置
};

// =============================================================================
// 模块3：声卡定义
// 作用：定义整个音频系统的拓扑结构
// =============================================================================

/*
 * DAI链路定义
 * DAI = Digital Audio Interface（数字音频接口）
 * 这个链路连接了CPU的I2S控制器和WM8960编解码器
 */
static struct snd_soc_dai_link wm8960_dai_link = {
	.name = "WM8960 Audio",           // 链路名称（用于调试）
	.stream_name = "WM8960 HiFi",     // 音频流名称（显示给用户）
	.ops = &wm8960_audio_ops,         // 支持的操作
	.dai_fmt = SND_SOC_DAIFMT_I2S |   // I2S格式
		   SND_SOC_DAIFMT_NB_NF |   // 正常位时钟和帧时钟
		   SND_SOC_DAIFMT_CBS_CFS,  // CPU提供位时钟和帧时钟
	.dpcm_playback = 1,               // 支持播放
	.dpcm_capture = 1,                // 支持录音
};

/*
 * 声卡结构体
 * 这是整个音频系统的"根"，包含所有组件的信息
 */
static struct snd_soc_card wm8960_sound_card = {
	.name = "RPi-WM8960",             // 声卡名称
	.owner = THIS_MODULE,             // 模块所有者
	.dai_link = &wm8960_dai_link,     // DAI链路
	.num_links = 1,                   // 链路数量
};

// =============================================================================
// 模块4：设备树解析
// 作用：从设备树读取硬件连接信息
// =============================================================================

/*
 * 解析设备树配置
 * 从设备树中读取I2S控制器和编解码器的信息
 */
static int wm8960_parse_dt(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;

	// 1. 找到I2S控制器节点
	struct device_node *i2s_node = of_parse_phandle(np, "i2s-controller", 0);
	if (!i2s_node) {
		dev_err(dev, "找不到i2s-controller配置\n");
		return -EINVAL;
	}

	// 2. 找到编解码器节点
	struct device_node *codec_node = of_parse_phandle(np, "audio-codec", 0);
	if (!codec_node) {
		dev_err(dev, "找不到audio-codec配置\n");
		return -EINVAL;
	}

	// 3. 配置CPU端（I2S控制器）
	wm8960_dai_link.cpus = devm_kzalloc(dev, sizeof(*wm8960_dai_link.cpus), GFP_KERNEL);
	if (!wm8960_dai_link.cpus)
		return -ENOMEM;

	wm8960_dai_link.cpus[0].of_node = i2s_node;
	wm8960_dai_link.num_cpus = 1;
	wm8960_dai_link.num_platforms = 1;
	wm8960_dai_link.platforms = wm8960_dai_link.cpus;

	// 4. 配置Codec端（WM8960）
	wm8960_dai_link.codecs = devm_kzalloc(dev, sizeof(*wm8960_dai_link.codecs), GFP_KERNEL);
	if (!wm8960_dai_link.codecs)
		return -ENOMEM;

	wm8960_dai_link.codecs[0].of_node = codec_node;
	wm8960_dai_link.codecs[0].dai_name = "wm8960-hifi";  // WM8960的标准DAI名称
	wm8960_dai_link.num_codecs = 1;

	dev_info(dev, "设备树解析完成：I2S控制器和WM8960编解码器已连接\n");
	return 0;
}

// =============================================================================
// 模块5：时钟管理
// 作用：管理WM8960的主时钟
// =============================================================================

/*
 * 初始化时钟
 * 查找并启用WM8960需要的主时钟
 */
static int wm8960_init_clock(struct platform_device *pdev,
			    struct wm8960_card_data *priv)
{
	struct device *dev = &pdev->dev;

	// 1. 首先尝试从编解码器节点获取时钟
	priv->mclk = of_clk_get_by_name(wm8960_dai_link.codecs[0].of_node, "mclk");

	// 2. 如果失败，尝试从根节点获取（兼容旧配置）
	if (IS_ERR(priv->mclk)) {
		priv->mclk = of_clk_get_by_name(of_find_node_by_path("/"), "wm8960_mclk");
	}

	// 3. 如果时钟存在，启用它
	if (!IS_ERR(priv->mclk)) {
		int ret = clk_prepare_enable(priv->mclk);
		if (ret) {
			dev_err(dev, "无法启用MCLK: %d\n", ret);
			clk_put(priv->mclk);
			priv->mclk = NULL;
			return ret;
		}
		priv->mclk_enabled = true;
		dev_info(dev, "MCLK已启用: %lu Hz\n", clk_get_rate(priv->mclk));
	} else {
		dev_warn(dev, "未找到MCLK，使用编解码器内部时钟\n");
	}

	return 0;
}

/*
 * 清理时钟
 * 禁用并释放时钟资源
 */
static void wm8960_cleanup_clock(struct wm8960_card_data *priv)
{
	if (priv->mclk_enabled && priv->mclk) {
		clk_disable_unprepare(priv->mclk);
		clk_put(priv->mclk);
		priv->mclk_enabled = false;
	}
}

// =============================================================================
// 模块6：驱动入口和出口
// 作用：Linux驱动的标准生命周期管理
// =============================================================================

/*
 * 驱动探测函数
 * 当设备树中匹配到我们的设备时，这个函数被调用
 * 作用：初始化整个音频系统
 */
static int wm8960_audio_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct wm8960_card_data *priv;
	int ret;

	dev_info(dev, "开始初始化WM8960音频驱动\n");

	// 1. 分配私有数据
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	// 2. 绑定私有数据到声卡
	snd_soc_card_set_drvdata(&wm8960_sound_card, priv);
	wm8960_sound_card.dev = dev;

	// 3. 解析设备树配置
	ret = wm8960_parse_dt(pdev);
	if (ret)
		return ret;

	// 4. 初始化时钟
	ret = wm8960_init_clock(pdev, priv);
	if (ret)
		return ret;

	// 5. 注册声卡到ASoC框架
	ret = devm_snd_soc_register_card(dev, &wm8960_sound_card);
	if (ret) {
		dev_err(dev, "无法注册声卡: %d\n", ret);
		wm8960_cleanup_clock(priv);
		return ret;
	}

	dev_info(dev, "WM8960音频驱动初始化完成\n");
	return 0;
}

/*
 * 驱动移除函数
 * 当驱动被卸载时调用，清理资源
 */
static void wm8960_audio_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct wm8960_card_data *priv = snd_soc_card_get_drvdata(card);

	dev_info(&pdev->dev, "卸载WM8960音频驱动\n");
	wm8960_cleanup_clock(priv);
}

// =============================================================================
// 模块7：设备树匹配表
// 作用：告诉Linux内核我们支持哪些设备树节点
// =============================================================================

/*
 * 设备树匹配表
 * 当设备树中出现compatible = "raspberrypi,rpi-wm8960-soundcard"时
 * Linux就会调用我们的驱动
 */
static const struct of_device_id wm8960_machine_of_match[] = {
	{ .compatible = "raspberrypi,rpi-wm8960-soundcard" },
	{ }
};
MODULE_DEVICE_TABLE(of, wm8960_machine_of_match);

/*
 * 平台驱动结构体
 * 这是Linux平台驱动的标准结构
 */
static struct platform_driver wm8960_machine_driver = {
	.driver = {
		.name = "rpi-wm8960-machine",           // 驱动名称
		.of_match_table = wm8960_machine_of_match, // 设备树匹配表
	},
	.probe = wm8960_audio_probe,     // 探测函数
	.remove = wm8960_audio_remove,   // 移除函数
};

/*
 * 模块初始化和退出
 * 这是Linux内核模块的标准入口和出口
 */
module_platform_driver(wm8960_machine_driver);

// =============================================================================
// 模块信息
// =============================================================================

MODULE_AUTHOR("Raspberry Pi Community");
MODULE_DESCRIPTION("WM8960音频驱动 - ASoC Machine Driver");
MODULE_LICENSE("GPL v2");
