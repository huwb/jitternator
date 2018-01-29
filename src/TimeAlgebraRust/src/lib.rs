pub fn run() {
    println!("Hello");
}

#[derive(Debug, Copy, Clone)]
pub struct Timed<T> {
    value: T,
    time: f32,
}

pub trait Linear {
    type Output;
    fn mul(&self, other: f32) -> Self::Output;
}
impl Linear for f32 {
    type Output = f32;
    fn mul(&self, other: f32) -> Self::Output {
        self * other
    }
}

// general types
impl<T> Timed<T> {
    pub fn sim_start(value: T) -> Timed<T> {
        Timed { value, time: 0.0 }
    }

    pub fn new(value: T, dt: &Dt) -> Timed<T> {
        Timed {
            value,
            time: dt.time,
        }
    }
}

// math ops
impl<T> Timed<T>
where
    T: Add<Output = T> + Mul<Output = T> + Linear<Output = T> + Copy,
{
    pub fn integrate(&mut self, deriv: &Timed<T>, dt: &Dt) {
        check_times(self, deriv);
        check_times(self, dt);
        self.value = self.value + Linear::mul(&deriv.value, dt.value);
        self.time = self.time + dt.value;
    }
}


type Dt = Timed<f32>;

impl Dt {
    pub fn advance(&mut self) {
        self.time += self.value;
    }

    pub fn finished_update(&mut self, dt: &Dt) {
        check_times(self, dt);
        self.time = self.time + dt.value;
    }
}

use std::ops::{Add, Div, Mul, Sub};

fn check_times<T, U>(a: &Timed<T>, b: &Timed<U>) {
    assert!(
        a.time == b.time,
        "Mismatched times: {} != {}",
        a.time,
        b.time
    );
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn dt_advance() {
        let mut dt = Dt::sim_start(1.0 / 32.0);
        dt.advance();
        assert_eq!(dt.time, dt.value);
    }

    #[test]
    fn simple_sim() {
        let mut dt = Dt::sim_start(1.0);
        let mut vel = Timed::sim_start(2.0);
        let mut pos = Timed::sim_start(1.0);

        for _i in 0..10 {
            // target comes from user input or animation, etc
            let target = Timed::new(10.0, &dt);

            let accel = target - pos;
            pos.integrate(&vel, &dt);
            vel.integrate(&accel, &dt);

            dt.advance();
        }

        assert_eq!(
            pos,
            Timed {
                value: 74.0,
                time: 10.0,
            }
        );
        assert_eq!(
            vel,
            Timed {
                value: 288.0,
                time: 10.0,
            }
        );
    }

    #[test]
    fn simple_sim_target_ahead() {
        let mut dt = Dt::sim_start(1.0 / 32.0);
        let mut vel = Timed::sim_start(2.0);
        let mut pos = Timed::sim_start(1.0);

        let mut target_last = Timed::sim_start(0.0);
        let mut target_last_valid = false;

        for _i in 0..10 {
            // target comes from user input or animation, etc
            let mut target = Timed::new(10.0, &dt);
            target.finished_update(&dt);

            // compute acceleration. defaults to 0 at time 0 - only valid
            // on first simulation step!
            let mut accel = Timed::sim_start(0.0);
            if target_last_valid {
                accel = target_last - pos;
            }
            target_last_valid = true;
            target_last = target;

            pos.integrate(&vel, &dt);
            vel.integrate(&accel, &dt);

            dt.advance();

            // (pretend to) modify dt based on measured real time passed in last iteration
            dt = Timed::new(dt.value + 0.1, &dt);
        }
    }

    #[test]
    fn add() {
        let s = Timed::sim_start(5);
        let t = Timed::sim_start(6);
        assert_eq!(s + t, Timed::sim_start(11));
    }

    #[test]
    fn sub() {
        let s = Timed::sim_start(5);
        let t = Timed::sim_start(6);
        assert_eq!(s - t, Timed::sim_start(-1));
    }

    #[test]
    fn mul() {
        let s = Timed::sim_start(5);
        let t = Timed::sim_start(6);
        assert_eq!(s * t, Timed::sim_start(30));
    }

    #[test]
    fn div() {
        let s = Timed::sim_start(6);
        let t = Timed::sim_start(2);
        assert_eq!(s / t, Timed::sim_start(3));
    }

    #[test]
    #[should_panic]
    fn add_mismatched_times_panics() {
        let s = Timed::sim_start(2.0);
        let mut t = Timed::sim_start(4.0);
        let dt = Dt::sim_start(1.0 / 32.0);
        t.finished_update(&dt);
        s + t;
    }

    #[test]
    #[should_panic]
    fn eq_mismatched_times_panics() {
        let s = Timed::sim_start(2.0);
        let mut t = Timed::sim_start(4.0);
        let dt = Dt::sim_start(1.0 / 32.0);
        t.finished_update(&dt);
        s == t;
    }
}


impl<T> Add for Timed<T>
where
    T: Add<Output = T>,
{
    type Output = Timed<T>;

    fn add(self, other: Timed<T>) -> Timed<T> {
        check_times(&self, &other);
        Timed {
            value: self.value.add(other.value),
            time: self.time,
        }
    }
}

impl<T> Sub for Timed<T>
where
    T: Sub<Output = T>,
{
    type Output = Timed<T>;

    fn sub(self, other: Timed<T>) -> Timed<T> {
        check_times(&self, &other);
        Timed {
            value: self.value.sub(other.value),
            time: self.time,
        }
    }
}

impl<T> Mul for Timed<T>
where
    T: Mul<Output = T>,
{
    type Output = Timed<T>;

    fn mul(self, other: Timed<T>) -> Timed<T> {
        check_times(&self, &other);
        Timed {
            value: self.value.mul(other.value),
            time: self.time,
        }
    }
}

impl<T> Div for Timed<T>
where
    T: Div<Output = T>,
{
    type Output = Timed<T>;

    fn div(self, other: Timed<T>) -> Timed<T> {
        check_times(&self, &other);
        Timed {
            value: self.value.div(other.value),
            time: self.time,
        }
    }
}

impl<T> PartialEq for Timed<T>
where
    T: PartialEq,
{
    fn eq(&self, other: &Timed<T>) -> bool {
        check_times(&self, &other);
        self.value == other.value
    }
}
