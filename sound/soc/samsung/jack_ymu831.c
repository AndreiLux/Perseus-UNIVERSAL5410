
/* To support PBA function test */
static struct class *jack_class;
static struct device *jack_dev;

static ssize_t earjack_state_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct snd_soc_codec *codec = dev_get_drvdata(dev);
	struct mc_asoc_priv *priv = snd_soc_codec_get_drvdata(codec);
	struct mc_asoc_jack *jack = &priv->data.jack;
	int report = 0;
	int status = jack->hs_jack->status;


	if ((status & SND_JACK_HEADPHONE) ||
		(status & SND_JACK_HEADSET)) {
		report = 1;
	}

	return sprintf(buf, "%d\n", report);
}

static ssize_t earjack_state_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_info("%s : operate nothing\n", __func__);

	return size;
}

static ssize_t earjack_key_state_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct snd_soc_codec *codec = dev_get_drvdata(dev);
	struct mc_asoc_priv *priv = snd_soc_codec_get_drvdata(codec);
	struct mc_asoc_jack *jack = &priv->data.jack;
	int report = 0;
	int status = jack->hs_jack->status;


	if (status & SND_JACK_BTN_0)
		report = 1;

	return sprintf(buf, "%d\n", report);
}

static ssize_t earjack_key_state_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_info("%s : operate nothing\n", __func__);

	return size;
}

static ssize_t earjack_select_jack_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_info("%s : operate nothing\n", __func__);

	return 0;
}

static ssize_t earjack_select_jack_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct snd_soc_codec *codec = dev_get_drvdata(dev);
	struct mc_asoc_priv *priv = snd_soc_codec_get_drvdata(codec);
	struct mc_asoc_jack *jack = &priv->data.jack;


	if ((!size) || (buf[0] != '1')) {
		snd_soc_jack_report(jack->hs_jack,
				    0, SND_JACK_HEADSET);
#ifdef CONFIG_SWITCH
		switch_set_state(jack->h2w_sdev, 0);
#endif
		pr_info("Forced remove microphone\n");
	} else {

		snd_soc_jack_report(jack->hs_jack,
				    SND_JACK_HEADSET, SND_JACK_HEADSET);

#ifdef CONFIG_SWITCH
		switch_set_state(jack->h2w_sdev, 1);
#endif
		pr_info("Forced detect microphone\n");
	}

	return size;
}

static DEVICE_ATTR(select_jack, S_IRUGO | S_IWUSR | S_IWGRP,
		   earjack_select_jack_show, earjack_select_jack_store);

static DEVICE_ATTR(key_state, S_IRUGO | S_IWUSR | S_IWGRP,
		   earjack_key_state_show, earjack_key_state_store);

static DEVICE_ATTR(state, S_IRUGO | S_IWUSR | S_IWGRP,
		   earjack_state_show, earjack_state_store);


static void create_jack_devices(struct snd_soc_codec *codec)
{
	/* To support PBA function test */
	jack_class = class_create(THIS_MODULE, "audio");

	if (IS_ERR(jack_class))
		pr_err("Failed to create class\n");

	jack_dev = device_create(jack_class, NULL, 0, codec, "earjack");

	if (device_create_file(jack_dev, &dev_attr_select_jack) < 0)
		pr_err("Failed to create device file (%s)!\n",
			dev_attr_select_jack.attr.name);

	if (device_create_file(jack_dev, &dev_attr_key_state) < 0)
		pr_err("Failed to create device file (%s)!\n",
			dev_attr_key_state.attr.name);

	if (device_create_file(jack_dev, &dev_attr_state) < 0)
		pr_err("Failed to create device file (%s)!\n",
			dev_attr_state.attr.name);
}
