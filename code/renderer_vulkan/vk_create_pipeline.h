#ifndef VK_CREATE_PIPELINES_H_
#define VK_CREATE_PIPELINES_H_


void create_standard_pipelines(void);
void create_pipelines_for_each_stage(shaderStage_t *pStage, shader_t* pShader);

void vk_destroyALLPipeline(void);
void R_PipelineList_f(void);

#endif
