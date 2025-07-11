module;

export module camera;

import std;

export namespace gcmt
{
	class camera
	{
	public:
		void translate(glm::vec3 delta_pos)
		{
			auto r = glm::mat4_cast(orientation);

			auto forward = glm::vec3{ r[0][2], r[1][2], r[2][2] };
			auto right   = glm::vec3{ r[0][0], r[1][0], r[2][0] };
			auto up_     = glm::vec3{ r[0][1], r[1][1], r[2][1] };

			position = (forward * delta_pos.z) +
			           (right * delta_pos.x) +
			           (up_ * delta_pos.y) +
			           position;
		}

		void rotate(glm::vec3 delta_angle)
		{
			//                                               Pitch           Yaw
			auto delta_orientation = glm::quat(glm::vec3{ delta_angle.x, delta_angle.y, 0.f });

			orientation = glm::normalize(delta_orientation * orientation);

			// reset up-vector
			//                                                     Roll
			delta_orientation = glm::quat(glm::vec3{ 0.f, 0.f, delta_angle.z });
			up                = glm::normalize(delta_orientation * up);

			auto r       = glm::mat4_cast(orientation);
			auto forward = glm::vec3{ r[0][2], r[1][2], r[2][2] };
			orientation  = glm::lookAt(position, position + forward, up);
		}

		void lookat(glm::vec3 eye, glm::vec3 target, glm::vec3 up_vec)
		{
			position = eye;
			up       = up_vec;

			auto view   = glm::lookAt(eye, target, up);
			orientation = glm::quat_cast(view);
		}

		[[nodiscard]] auto get_view() const -> glm::mat4
		{
			// TODO: add toggle for Free Camera, to Arc Camera
			// - remove negation of position in translation
			// - flip rotation * translation to translation * rotation

			auto translation = glm::translate(glm::mat4(1.0f), -position);
			auto rotation    = glm::mat4_cast(orientation);

			return rotation * translation;
		}

		[[nodiscard]] auto get_position() const -> glm::vec3
		{
			return position;
		}

	private:
		glm::vec3 position    = {};
		glm::vec3 up          = {};
		glm::quat orientation = {};
	};
}