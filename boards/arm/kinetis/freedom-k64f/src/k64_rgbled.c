/****************************************************************************
 * boards/arm/kinetis/freedom-k64f/src/k64_rgbled.c
 *
 * A "fake PWM" lower-half implementation for the FRDM-K64F RGB LED.
 * The on-board RGB LED is GPIO-only (active low).  To provide a compatible
 * interface to the NuttX rgbled subsystem (which expects PWM lower-halves),
 * we implement minimal pwm_lowerhalf_s objects that toggle GPIOs based on
 * the requested duty cycle (0 = off, >0 = on).
 *
 * This allows registering the RGB LED at "/dev/rgbled0" via rgbled_register()
 * even though we do not provide hardware PWM brightness control.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <debug.h>

#include <nuttx/timers/pwm.h>
#include <nuttx/leds/rgbled.h>

#include <arch/board/board.h>

#include "kinetis.h"
#include "freedom-k64f.h"

#ifdef CONFIG_RGBLED

/* Forward declarations */

static int fake_pwm_setup(struct pwm_lowerhalf_s *dev);
static int fake_pwm_shutdown(struct pwm_lowerhalf_s *dev);
static int fake_pwm_start(struct pwm_lowerhalf_s *dev,
                          const struct pwm_info_s *info);
static int fake_pwm_stop(struct pwm_lowerhalf_s *dev);
static int fake_pwm_ioctl(struct pwm_lowerhalf_s *dev, int cmd,
                          unsigned long arg);

/* pwm ops structure */

static const struct pwm_ops_s fake_pwm_ops =
{
  .setup    = fake_pwm_setup,
  .shutdown = fake_pwm_shutdown,
  .start    = fake_pwm_start,
  .stop     = fake_pwm_stop,
  .ioctl    = fake_pwm_ioctl,
};

/* Private data for each fake PWM channel (one per LED) */

struct fake_pwm_s
{
  FAR const struct pwm_ops_s   *ops;      /* must be first for pwm_lowerhalf_s */
  uint32_t                     pincfg;    /* GPIO config macro for the LED */
  struct pwm_info_s            last;      /* last configured freq/duty */
  bool                         running;   /* started flag */
};

/* Instantiate one fake PWM lower-half per LED */

static struct fake_pwm_s g_fake_pwm_r =
{
  .ops      = &fake_pwm_ops,
  .pincfg   = GPIO_LED_R,
  .last     = { .frequency = 0, .duty = 0 },
  .running  = false,
};

static struct fake_pwm_s g_fake_pwm_g =
{
  .ops      = &fake_pwm_ops,
  .pincfg   = GPIO_LED_G,
  .last     = { .frequency = 0, .duty = 0 },
  .running  = false,
};

static struct fake_pwm_s g_fake_pwm_b =
{
  .ops      = &fake_pwm_ops,
  .pincfg   = GPIO_LED_B,
  .last     = { .frequency = 0, .duty = 0 },
  .running  = false,
};

/* Helper: set the GPIO according to duty (active low):
 * duty == 0 -> LED off (write HIGH)
 * duty > 0  -> LED on  (write LOW)
 */

static inline void fake_pwm_apply_pin(struct fake_pwm_s *p)
{
  bool on = (p->last.duty > 0);
  kinetis_gpiowrite(p->pincfg, on ? true : false ? false : true);
}

/* Minimal implementation of PWM ops */

/* setup: configure the GPIO as output (idle off) */
static int fake_pwm_setup(struct pwm_lowerhalf_s *dev)
{
  struct fake_pwm_s *priv = (struct fake_pwm_s *)dev;

  /* Configure pin as GPIO output (initialized to OFF = HIGH because active low) */
  kinetis_pinconfig(priv->pincfg);
  kinetis_gpiowrite(priv->pincfg, true); /* OFF (active low) */

  priv->last.frequency = 0;
  priv->last.duty = 0;
  priv->running = false;

  return OK;
}

/* shutdown: return pin to default (keep as output and off) */
static int fake_pwm_shutdown(struct pwm_lowerhalf_s *dev)
{
  struct fake_pwm_s *priv = (struct fake_pwm_s *)dev;

  /* Turn LED off and mark not running */
  kinetis_gpiowrite(priv->pincfg, true);
  priv->running = false;
  priv->last.frequency = 0;
  priv->last.duty = 0;

  return OK;
}

/* start: store pwm_info and apply on/off accordingly */
static int fake_pwm_start(struct pwm_lowerhalf_s *dev, const struct pwm_info_s *info)
{
  struct fake_pwm_s *priv = (struct fake_pwm_s *)dev;

  if (!info)
    {
      return -EINVAL;
    }

  /* Accept the requested frequency but we don't use it for GPIO-only LED */
  priv->last.frequency = info->frequency;
  priv->last.duty = info->duty;
  priv->running = true;

  /* Apply on/off based on duty (0 == off, >0 == on) */
  /* Because LEDs are active-low, we write LOW when "on" */
  if (info->duty == 0)
    {
      /* OFF */
      kinetis_gpiowrite(priv->pincfg, true); /* HIGH -> LED off */
    }
  else
    {
      /* ON */
      kinetis_gpiowrite(priv->pincfg, false); /* LOW -> LED on */
    }

  return OK;
}

/* stop: turn off the LED */
static int fake_pwm_stop(struct pwm_lowerhalf_s *dev)
{
  struct fake_pwm_s *priv = (struct fake_pwm_s *)dev;

  /* Turn LED OFF (active low => write HIGH) */
  kinetis_gpiowrite(priv->pincfg, true);
  priv->running = false;
  priv->last.duty = 0;

  return OK;
}

/* ioctl: support minimal queries for PWM_INFO */
static int fake_pwm_ioctl(struct pwm_lowerhalf_s *dev, int cmd, unsigned long arg)
{
  struct fake_pwm_s *priv = (struct fake_pwm_s *)dev;

  switch (cmd)
    {
      case PWMIOC_SETCHARACTERISTICS:
        {
          /* arg is a pointer to struct pwm_info_s: update stored values */
          FAR struct pwm_info_s *info = (FAR struct pwm_info_s *)((uintptr_t)arg);
          if (!info)
            {
              return -EINVAL;
            }

          priv->last.frequency = info->frequency;
          priv->last.duty = info->duty;
          if (priv->running)
            {
              /* apply immediately */
              if (info->duty == 0)
                kinetis_gpiowrite(priv->pincfg, true);
              else
                kinetis_gpiowrite(priv->pincfg, false);
            }
          return OK;
        }

      case PWMIOC_GETCHARACTERISTICS:
        {
          FAR struct pwm_info_s *info = (FAR struct pwm_info_s *)((uintptr_t)arg);
          if (!info)
            {
              return -EINVAL;
            }
          info->frequency = priv->last.frequency;
          info->duty = priv->last.duty;
          return OK;
        }

      default:
        return -ENOTTY;
    }
}

/****************************************************************************
 * Public function: k64_rgbled_setup
 *
 * Register the RGB driver using the fake PWM lower-halves so that the
 * rgbled subsystem can be used by applications (via /dev/rgbled0).
 ****************************************************************************/

int k64_rgbled_setup(void)
{
  static bool initialized = false;
  int ret;

  if (!initialized)
    {
      /* Ensure the GPIO pins are configured (setup will also do this) */
      fake_pwm_setup((struct pwm_lowerhalf_s *)&g_fake_pwm_r);
      fake_pwm_setup((struct pwm_lowerhalf_s *)&g_fake_pwm_g);
      fake_pwm_setup((struct pwm_lowerhalf_s *)&g_fake_pwm_b);

      /* Register with rgbled_register() - the rgbled code will call
       * the pwm_lowerhalf_s ops (start/stop/etc.) on these objects.
       */
      ret = rgbled_register("/dev/rgbled0",
                            (struct pwm_lowerhalf_s *)&g_fake_pwm_r,
                            (struct pwm_lowerhalf_s *)&g_fake_pwm_g,
                            (struct pwm_lowerhalf_s *)&g_fake_pwm_b);

      if (ret < 0)
        {
          lederr("ERROR: rgbled_register failed: %d\n", ret);
          return ret;
        }

      initialized = true;
    }

  return OK;
}

#endif /* CONFIG_RGBLED */
