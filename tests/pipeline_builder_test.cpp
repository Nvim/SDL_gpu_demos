#include "common/pipeline_builder.h"
#include <catch2/catch_test_macros.hpp>

SCENARIO("PipelineBuilder initial state is 0", "[pipeline_builder]")
{
  GIVEN("A new PipelineBuilder")
  {
    auto builder = PipelineBuilder{};

    THEN("All fields should be properly initialized")
    {
      auto& i = builder.pipeline_info;
      REQUIRE(i.vertex_input_state.num_vertex_buffers == 1);
      REQUIRE(i.vertex_input_state.vertex_attributes == nullptr);
      REQUIRE(i.vertex_input_state.vertex_buffer_descriptions != nullptr);
      REQUIRE(i.target_info.color_target_descriptions != nullptr);
    }
  }
}

SCENARIO("PipelineBuilder can be reset", "[pipeline_builder]")
{
  GIVEN("A used PipelineBuilder")
  {
    auto builder = PipelineBuilder{};
    SDL_GPUShader* invalid_shader = (SDL_GPUShader*)(&builder);
    builder //
      .AddColorTarget(SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT, false)
      .SetPrimitiveType(SDL_GPU_PRIMITIVETYPE_LINELIST)
      .SetVertexShader(invalid_shader)
      .EnableDepthWrite()
      .EnableDepthTest()
      .AddVertexAttribute(SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3)
      .AddVertexAttribute(SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4);

    WHEN("The builder is reset")
    {
      builder.Reset();
      THEN("All modified fields should be reset")
      {
        auto& i = builder.pipeline_info;
        REQUIRE(i.target_info.num_color_targets == 0);
        REQUIRE_FALSE(i.target_info.has_depth_stencil_target);
        REQUIRE_FALSE(i.depth_stencil_state.enable_depth_test);
        REQUIRE_FALSE(i.depth_stencil_state.enable_depth_write);
        REQUIRE(i.primitive_type == SDL_GPU_PRIMITIVETYPE_TRIANGLELIST);
        REQUIRE(i.vertex_shader == nullptr);
        REQUIRE(i.vertex_input_state.num_vertex_attributes == 0);
      }
    }
  }
}

SCENARIO("PipelineBuilder vertex attributes counters", "[pipeline_builder]")
{
  GIVEN("A PipelineBuidler with no vertex attributes")
  {
    auto builder = PipelineBuilder{};
    THEN("vertex_attrs_offset should be 0")
    {
      REQUIRE(builder.vertex_attrs_offset == 0);
    }
    THEN("vertex_attributes should be empty")
    {
      REQUIRE(builder.vertex_attributes.size() == 0);
    }
    THEN("Vertex buffer description's pitch should be 0")
    {
      REQUIRE(builder.vert_desc.pitch == 0);
    }

    static constexpr auto FLOAT_3 = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    static constexpr auto FLOAT_2 = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    WHEN("A 3-component float attribute is added")
    {
      builder.AddVertexAttribute(FLOAT_3);

      THEN("vertex_attrs_offset should be 12")
      {
        REQUIRE(builder.vertex_attrs_offset == 12);
      }
      THEN("vertex_attributes shouldn't be empty")
      {
        REQUIRE(builder.vertex_attributes.size() == 1);
        REQUIRE(
          builder.pipeline_info.vertex_input_state.num_vertex_attributes == 1);
      }
      THEN("the vertex attribute should have correct fields")
      {
        REQUIRE(builder.vertex_attributes[0].location == 0);
        REQUIRE(builder.vertex_attributes[0].buffer_slot == 0);
        REQUIRE(builder.vertex_attributes[0].offset == 0);
        REQUIRE(builder.vertex_attributes[0].format == FLOAT_3);
      }
      THEN("Vertex buffer description's pitch should be 12")
      {
        REQUIRE(builder.vert_desc.pitch == 12);
      }

      AND_WHEN("A 2-component float attribute is added")
      {
        builder.AddVertexAttribute(FLOAT_2);

        THEN("vertex_attrs_offset should be 12 + 8")
        {
          REQUIRE(builder.vertex_attrs_offset == 12 + 8);
        }
        THEN("vertex_attributes should be 2")
        {
          REQUIRE(builder.vertex_attributes.size() == 2);
          REQUIRE(
            builder.pipeline_info.vertex_input_state.num_vertex_attributes ==
            2);
        }
        THEN("the vertex attribute should have correct fields")
        {
          REQUIRE(builder.vertex_attributes[1].location == 1);
          REQUIRE(builder.vertex_attributes[1].buffer_slot == 0);
          REQUIRE(builder.vertex_attributes[1].offset == 12);
          REQUIRE(builder.vertex_attributes[1].format == FLOAT_2);
        }
        THEN("Vertex buffer description's pitch should be 12 + 8")
        {
          REQUIRE(builder.vert_desc.pitch == 12 + 8);
        }
      }
    }
  }
}

SCENARIO("PipelineBuilder color targets number is capped", "[pipeline_builder]")
{
  REQUIRE(PipelineBuilder::MAX_COLOR_TARGETS == 4);
  GIVEN("A PipelineBuilder with 4 color targets")
  {
    auto builder = PipelineBuilder{};
    builder.AddColorTarget(SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, false);
    builder.AddColorTarget(SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, true);
    builder.AddColorTarget(SDL_GPU_TEXTUREFORMAT_R32G32_FLOAT, false);
    builder.AddColorTarget(SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT, true);

    THEN("num_color_targets should be 4")
    {
      REQUIRE(builder.num_color_targets == 4);
    }

    WHEN("Adding a new target")
    {
      builder.AddColorTarget(SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB, false);

      THEN("num_colors_targets should still be 4")
      {
        REQUIRE(builder.num_color_targets == 4);
      }
    }
  }
}
